/* Build 26.10.01.3 05:02PM */
/*****************************************************************************
 *   AVDEFS.H                                Auto Validator - Shared defines  *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1995 High Velocity Software, Inc.              *
 *                                                                           *
 *   Declarations shared by every Auto Validator source file: the version,   *
 *   the per-user session scratch area, the user-state numbers, and the      *
 *   prototypes of each source file's public routines.                       *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0 - see the     *
 *   LICENSE file in the project root.                                       *
 *****************************************************************************/

#ifndef AVDEFS_H
#define AVDEFS_H

#include "gcomm.h"
#include "majorbbs.h"
#include "HVSAV.H"                   /* message numbers for HVSAV.MSG       */

/* ------------------------------------------------------------------------ *
 * Version.  HVS_VERSION is what sysops see (audit trail, sysop menu); the   *
 * numeric parts feed the DLL's Windows version resource (HVSAV.rc).          *
 * ------------------------------------------------------------------------ */
#define HVS_VER_MAJOR   2
#define HVS_VER_MINOR   1
#define HVS_VER_PATCH   0
#define HVS_VERSION     "2.1.0"

#define HVS_NMETH       5            /* validation methods 1..5              */
#define HVS_NOCODE      (-1)         /* "no code issued" marker in a record  */

/* ------------------------------------------------------------------------ *
 * User states (usrptr->substt).  0 is "just arrived".  The same states are  *
 * used whether the user came in through the menu or through the logon        *
 * offer, so one input routine serves both.                                   *
 * ------------------------------------------------------------------------ */
#define ST_ENTRY        0            /* first call: decide what to do        */
#define ST_LOGONQ       1            /* logon: "validate now (Y/N)?"         */
#define ST_EMLCONT      2            /* "begin email validation (Y/N)?"      */
#define ST_EMLADDR      3            /* waiting for an email address         */
#define ST_CODE         4            /* waiting for the emailed code         */
#define ST_SYSMENU      10           /* sysop menu                           */
#define ST_SETMENU      11           /* settings menu                        */
#define ST_SETPAGE      12           /* a page of settings                   */
#define ST_SETVAL       13           /* new value for one setting            */
#define ST_SETRST       14           /* confirm reset to defaults            */
#define ST_LOOKUP       15           /* user-id to look up                   */

/* ------------------------------------------------------------------------ *
 * Per-user scratch area.  The engine gives each channel a "volatile data"   *
 * area (vdaptr) for whatever module the user is in; we size it with         *
 * dclvda() at startup and clear it each time a user enters.                  *
 * ------------------------------------------------------------------------ */
struct avvda {
     INT tries;                      /* wrong codes entered this visit       */
     INT page;                       /* settings page: 0=general, 1-5=method */
     INT item;                       /* setting being edited (table index)   */
};
#define avv ((struct avvda *)vdaptr)

/* ---------------------------------------------------------------- HVSAV.C */
extern HMCVFILE avmb;                /* our message file                     */

/* ---------------------------------------------------------------- AVLOG.C */
VOID av_log(const CHAR *fmt, ...);   /* append a line to the activity log    */

/* -------------------------------------------------------------- AVTVARS.C */
VOID tv_init(VOID);                  /* register our text variables          */
VOID tv_set(INT which, const CHAR *value);
VOID tv_setn(INT which, LONG value);
VOID tv_raw(GBOOL raw);              /* TRUE: no %% escaping (non-prfmsg use)*/

enum {                               /* text variable slots                  */
     TV_USERID, TV_EMAIL, TV_EMAILTYP, TV_CODE, TV_CREDITS, TV_COUNT,
     TV_ATTEMPTS, TV_VALID, TV_OVERRIDE, TV_METHOD, TV_ITEM, TV_LABEL,
     TV_VALUE, TV_PAGE, TV_LIMIT, TV_GLOB,
     TV_COUNT_OF
};

/* -------------------------------------------------------------- AVVALID.C */
GBOOL av_begin(GBOOL fromlogon);     /* start the user validation dialog     */
GBOOL av_input(VOID);                /* continue it with the user's input    */
GBOOL av_logon(VOID);                /* logon offer (lonrou)                 */
GBOOL av_sendcode(const CHAR *userid);         /* (re)send a code by email   */
GBOOL av_validate(const CHAR *userid, INT method, GBOOL forced);

/* -------------------------------------------------------------- AVSYSOP.C */
GBOOL av_issysop(VOID);              /* does the user hold the sysop key?    */
GBOOL sy_begin(VOID);                /* show the sysop menu                  */
GBOOL sy_input(VOID);                /* sysop menu input                     */
INT   sy_global(VOID);               /* global command handler               */
VOID  sy_userinfo(const CHAR *userid);         /* print a user's record      */

#endif /* AVDEFS_H */
