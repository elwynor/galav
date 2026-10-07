/* Build 26.10.07.1 03:36PM */
/*****************************************************************************
 *   GALAV.C   v2.2.1                    Auto Validator for The Major BBS v10 *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1993-1996 High Velocity Software, Inc.         *
 *   Auto Validator converted for The Major BBS v10 by Mark Laudenbach -     *
 *   https://github.com/laudenbachm                                          *
 *                                                                           *
 *   Module entry point.  Registers the module with the BBS, opens its       *
 *   files, and routes each line a user types to the user side (AVVALID.C)  *
 *   or the sysop side (AVSYSOP.C).                                          *
 *                                                                           *
 *   Source map:                                                             *
 *     GALAV.C    this file - startup, shutdown, input routing               *
 *     AVVALID.C  the user's email validation dialog; applying a method      *
 *     AVMAIL.C   the validation email: From, subject, body; online editor   *
 *     AVSYSOP.C  sysop menu, online settings editor, global commands        *
 *     AVSET.C    live settings (GALAVSET.DAT) and their defaults           *
 *     AVUSER.C   per-user validation records (GALAVUSR.DAT)                 *
 *     AVTVARS.C  named text variables used in GALAV.MSG                     *
 *     AVLOG.C    the activity log file                                      *
 *     GALAV.H    message numbers for GALAV.MSG                              *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0 - see the     *
 *   LICENSE file in the project root.                                       *
 *****************************************************************************/

#include <stdio.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "AVSET.H"
#include "AVUSER.H"

static GBOOL avlogon(VOID);
static GBOOL avinput(VOID);
static VOID  avdelete(CHAR *userid);
static VOID  avshutdown(VOID);

static struct module galav = {       /* our entry in the BBS module table    */
     "",                             /*   name (filled from GALAV.MDF)       */
     avlogon,                        /*   logon: optional offer to validate  */
     avinput,                        /*   a line of input while in module    */
     NULL,                           /*   status input (engine default)      */
     NULL,                           /*   inject-other-user message          */
     NULL,                           /*   logoff                             */
     NULL,                           /*   hang-up                            */
     NULL,                           /*   midnight cleanup                   */
     avdelete,                       /*   account deleted                    */
     avshutdown                      /*   system shutdown                    */
};

HMCVFILE avmb;                       /* GALAV.MCV, our message file          */
INT avstt;                           /* our module (state) number            */

static VOID
migrate(                             /* take over a file from the HVSAV days */
const CHAR *oldnam,                  /*   2.1.0 and earlier name             */
const CHAR *newnam)                  /*   current name                       */
{
     /* Up to 2.1.0 the module's files were named HVSAV*.  Rename a board's */
     /* existing ones once, so validation records and settings carry over.  */
     if (!isfile(newnam) && isfile(oldnam) && rename(oldnam, newnam) == 0) {
          shocst("GALAV FILE RENAMED", "%s renamed to %s", oldnam, newnam);
     }
}

VOID EXPORT
init__galav(VOID)                    /* called once by the BBS at startup    */
{
     stzcpy(galav.descrp, gmdnam("GALAV.MDF"), MNMSIZ);
     avstt = register_module(&galav);
     avmb = opnmsg("GALAV.MCV");
     dclvda(sizeof(struct avvda));   /* per-user scratch area we need        */
     tv_init();
     migrate("HVSAVUSR.DAT", "GALAVUSR.DAT");
     migrate("HVSAVSET.DAT", "GALAVSET.DAT");
     migrate("HVSAVBAD.TXT", "GALAVBAD.TXT");
     set_open();                     /* settings: load, or seed from the MSG */
     avu_open();
     globalcmd(sy_global);

     /* Name the running build in the Audit Trail so a sysop can always    */
     /* tell which version is loaded.                                       */
     shocst(spr("AUTO VALIDATOR v%s", GALAV_VERSION),
            "(C) Elwynor Technologies; orig. High Velocity Software");
     av_log("STARTUP: Auto Validator v%s started", GALAV_VERSION);
}

static GBOOL
avinput(VOID)                        /* a line of input while in the module  */
{
     GBOOL more;

     setmbk(avmb);
     if (usrptr->substt == ST_ENTRY) {          /* just arrived              */
          setmem(vdaptr, sizeof(struct avvda), 0);
          more = av_issysop() ? sy_begin() : av_begin(FALSE);
     }
     else if (usrptr->substt >= ST_SYSMENU) {
          more = sy_input();
     }
     else {
          more = av_input();
     }
     outprf(usrnum);
     rstmbk();
     return more;                    /* FALSE returns the user to the menu   */
}

static GBOOL
avlogon(VOID)                        /* logon supplement: offer validation   */
{
     GBOOL more;

     /* Returning TRUE keeps the user here during logon: the engine sends   */
     /* their next line of input back to this routine until we return FALSE.*/
     setmbk(avmb);
     if (usrptr->substt == ST_ENTRY) {
          setmem(vdaptr, sizeof(struct avvda), 0);
          more = av_logon();
     }
     else {
          more = av_input();
     }
     outprf(usrnum);
     rstmbk();
     return more;
}

static VOID
avdelete(CHAR *userid)               /* a BBS account was deleted            */
{
     avu_delete(userid);
}

static VOID
avshutdown(VOID)                     /* the BBS is shutting down             */
{
     avu_close();
     set_close();
     clsmsg(avmb);
}
