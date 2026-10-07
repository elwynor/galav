/* Build 26.10.07.1 03:36PM */
/*****************************************************************************
 *   AVSYSOP.C                       Auto Validator - Sysop menu & commands   *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1995 High Velocity Software, Inc.              *
 *                                                                           *
 *   What a user holding the sysop key (CNFGACES setting) sees:              *
 *     - the sysop menu, with the online settings editor and user look-up;   *
 *     - the global commands (@V, @D, @E, @L, @O, @?), usable from anywhere  *
 *       on the system.                                                      *
 *   Settings changes take effect at once and are written to the Audit       *
 *   Trail and the activity log.                                             *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "AVSET.H"
#include "AVUSER.H"

GBOOL
av_issysop(VOID)                     /* may this user run the validator?     */
{
     /* An empty sysop-key setting falls back to SYSOP, so clearing it can   */
     /* never lock every sysop out.                                          */
     return haskey(avcfg.cfgkey[0] != '\0' ? avcfg.cfgkey : "SYSOP");
}

static CHAR
choice(VOID)                         /* first character typed, upper case    */
{
     return margc > 0 ? (CHAR)toupper((UCHAR)margv[0][0]) : '\0';
}

static GBOOL
findacct(                            /* find a BBS account by user-id        */
const CHAR *typed,                   /*   as the sysop typed it              */
CHAR *canon)                         /*   receives the exact user-id         */
{
     static struct usracc acc;
     CHAR key[UIDSIZ];
     GBOOL found;

     stzcpy(key, typed, UIDSIZ);
     dfaSetBlk(accbb);
     found = dfaAcqEQ(&acc, key, 0);
     dfaRstBlk();
     if (found) {
          stzcpy(canon, acc.userid, UIDSIZ);
     }
     return found;
}

static VOID
setglob(VOID)                        /* fill GALAV_GLOB with trigger char    */
{
     CHAR g[2];

     g[0] = avcfg.globchr;
     g[1] = '\0';
     tv_set(TV_GLOB, g);
}

/* ------------------------------------------------------------------------ *
 * User look-up (sysop menu "L" and the @L command).                        *
 * ------------------------------------------------------------------------ */

VOID
sy_userinfo(const CHAR *typed)       /* show a user's validation record      */
{
     struct avuser u;
     CHAR uid[UIDSIZ];

     tv_set(TV_USERID, typed);
     if (!findacct(typed, uid)) {
          prfmsg(NOUSER);
          return;
     }
     tv_set(TV_USERID, uid);
     if (!avu_get(uid, &u)) {
          prfmsg(NOAVREC);
          return;
     }
     tv_set(TV_EMAIL, u.email[0] != '\0' ? u.email : "(none)");
     tv_set(TV_VALID, u.validated ? "Yes" : "No");
     tv_set(TV_CODE, u.code == GALAV_NOCODE ? "(none)"
                                          : spr("%04X", (USHORT)u.code));
     tv_setn(TV_METHOD, u.method);
     tv_setn(TV_ATTEMPTS, u.attempts);
     tv_set(TV_OVERRIDE, u.override ? "On" : "Off");
     tv_setn(TV_COUNT, avu_emailcount(u.email, u.userid));
     prfmsg(USERINFO);
}

/* ------------------------------------------------------------------------ *
 * The sysop menu and settings editor.                                      *
 * ------------------------------------------------------------------------ */

static GBOOL
showmenu(VOID)
{
     prfmsg(SYSMENU);
     prfmsg(SYSPRMPT);
     usrptr->substt = ST_SYSMENU;
     return TRUE;
}

GBOOL
sy_emlmenu(VOID)                     /* the validation email menu            */
{
     tv_set(TV_FROM, avcfg.fromadr[0] != '\0' ? avcfg.fromadr
                                              : "Sysop at your SMTP host name");
     tv_set(TV_SUBJECT, avcfg.emlsubj);
     tv_set(TV_BODYSRC, ml_custom() ? "custom (GALAVEML.TXT)"
                                    : "default (YOUVAL in GALAV.MSG)");
     prfmsg(EMLMENU);
     prfmsg(EMLPRMPT);
     usrptr->substt = ST_EMLMENU;
     return TRUE;
}

static GBOOL
showsetmenu(VOID)
{
     prfmsg(SETMENU);
     prfmsg(SETPRMPT);
     usrptr->substt = ST_SETMENU;
     return TRUE;
}

static GBOOL
showpage(INT page)                   /* list one page of settings            */
{
     INT n, idx;

     avv->page = page;
     tv_set(TV_PAGE, set_title(page));
     prfmsg(SETHDR);
     for (n = 1 ; n <= set_count(page) ; n++) {
          idx = set_index(page, n);
          tv_setn(TV_ITEM, n);
          tv_set(TV_LABEL, set_label(idx));
          tv_set(TV_VALUE, set_value(idx));
          prfmsg(SETROW);
     }
     prfmsg(SETPGPR);
     usrptr->substt = ST_SETPAGE;
     return TRUE;
}

static GBOOL
askvalue(VOID)                       /* prompt for the setting being edited  */
{
     tv_set(TV_LABEL, set_label(avv->item));
     tv_set(TV_VALUE, set_value(avv->item));
     tv_set(TV_LIMIT, set_limit(avv->item));
     prfmsg(SETVALPR);
     usrptr->substt = ST_SETVAL;
     return TRUE;
}

static GBOOL
gotvalue(VOID)                       /* ST_SETVAL: the sysop typed a value   */
{
     CHAR text[INPSIZ], old[64];

     rstrin();
     stzcpy(text, skpwht(input), sizeof(text));
     unpad(text);
     stzcpy(old, set_value(avv->item), sizeof(old));
     switch (set_change(avv->item, text)) {
     case SETCHG_OK:
          tv_set(TV_LABEL, set_label(avv->item));
          tv_set(TV_VALUE, set_value(avv->item));
          prfmsg(SETSAVED);
          shocst("GALAV SETTING CHANGED", "%s: %s = %s", usaptr->userid,
                 set_label(avv->item), set_value(avv->item));
          av_log("SETTING: %s changed \"%s\" from %s to %s", usaptr->userid,
                 set_label(avv->item), old, set_value(avv->item));
          return showpage(avv->page);
     case SETCHG_BAD:
          tv_set(TV_LIMIT, set_limit(avv->item));
          prfmsg(SETBAD);
          return askvalue();
     case SETCHG_NOCLS:
          tv_set(TV_VALUE, text);
          prfmsg(SETNOCLS);
          return askvalue();
     }
     return showpage(avv->page);     /* SETCHG_SAME: nothing typed           */
}

GBOOL
sy_begin(VOID)                       /* a sysop has entered the module       */
{
     return showmenu();
}

GBOOL
sy_input(VOID)                       /* one line of sysop menu input         */
{
     INT n, idx;
     CHAR uid[INPSIZ];

     switch (usrptr->substt) {
     case ST_SYSMENU:
          switch (choice()) {
          case 'S':
               return showsetmenu();
          case 'E':
               return sy_emlmenu();
          case 'L':
               prfmsg(LKUPPR);
               usrptr->substt = ST_LOOKUP;
               return TRUE;
          case 'G':
               setglob();
               prfmsg(GLBLCMND);
               prfmsg(SYSPRMPT);
               return TRUE;
          case '?':
               prfmsg(SYSHELP);
               return showmenu();
          case 'X':
               return FALSE;
          }
          return showmenu();

     case ST_SETMENU:
          n = choice();
          if (n == 'G') {
               return showpage(SETPG_GENERAL);
          }
          if (n >= '1' && n < '1' + GALAV_NMETH) {
               return showpage(n - '0');
          }
          if (n == 'R') {
               prfmsg(SETRSTQ);
               usrptr->substt = ST_SETRST;
               return TRUE;
          }
          if (n == 'X') {
               return showmenu();
          }
          return showsetmenu();

     case ST_SETPAGE:
          if (choice() == 'X') {
               return showsetmenu();
          }
          idx = margc > 0 ? set_index(avv->page, atoi(margv[0])) : -1;
          if (idx < 0) {
               return showpage(avv->page);
          }
          avv->item = idx;
          return askvalue();

     case ST_SETVAL:
          return gotvalue();

     case ST_SETRST:
          if (choice() == 'Y') {
               set_defaults();
               prfmsg(SETRSTD);
               shocst("GALAV SETTINGS RESET",
                      "%s reset settings to the defaults", usaptr->userid);
               av_log("SETTING: %s reset all settings to the defaults",
                      usaptr->userid);
          }
          return showsetmenu();

     case ST_EMLMENU:
          switch (choice()) {
          case 'E':
               ml_edit();            /* edone() in AVMAIL.C brings them back */
               return TRUE;
          case 'R':
               prfmsg(EMLRSTQ);
               usrptr->substt = ST_EMLRST;
               return TRUE;
          case 'X':
               return showmenu();
          }
          return sy_emlmenu();

     case ST_EMLRST:
          if (choice() == 'Y') {
               ml_restore();
               prfmsg(EMLRSTD);
               shocst("GALAV EMAIL RESET",
                      "%s restored the default validation email", usaptr->userid);
               av_log("SETTING: %s restored the default validation email",
                      usaptr->userid);
          }
          return sy_emlmenu();

     case ST_LOOKUP:
          rstrin();
          stzcpy(uid, skpwht(input), sizeof(uid));
          unpad(uid);
          if (uid[0] == '\0' || sameas(uid, "X")) {
               return showmenu();
          }
          sy_userinfo(uid);
          prfmsg(LKUPPR);
          return TRUE;
     }
     return FALSE;
}

/* ------------------------------------------------------------------------ *
 * Global commands.  The engine offers every line any user types to each    *
 * registered global handler; we claim it only for a sysop typing our       *
 * trigger character plus one of our command letters as the first word.    *
 * ------------------------------------------------------------------------ */

static GBOOL
needuser(                            /* resolve the user a command names     */
const CHAR *rest,                    /*   text after the command             */
INT syntax,                          /*   message to show if none given      */
CHAR *uid)                           /*   receives the exact user-id         */
{
     if (*rest == '\0') {
          prfmsg(syntax);
          return FALSE;
     }
     tv_set(TV_USERID, rest);
     if (!findacct(rest, uid)) {
          prfmsg(NOUSER);
          return FALSE;
     }
     tv_set(TV_USERID, uid);
     return TRUE;
}

static VOID
gcmd(CHAR cmd, CHAR *rest)           /* carry out one global command         */
{
     struct avuser u;
     CHAR uid[UIDSIZ];
     INT method;

     switch (cmd) {
     case 'V':                       /* @V n user - validate with method n   */
          if (rest[0] < '1' || rest[0] >= '1' + GALAV_NMETH
           || !isspace((UCHAR)rest[1])) {
               prfmsg(WRNGSNTX);
               return;
          }
          method = rest[0] - '0';
          if (!needuser(skpwht(rest + 1), WRNGSNTX, uid)) {
               return;
          }
          if (!av_validate(uid, method, TRUE)) {
               tv_set(TV_VALUE, avcfg.meth[method - 1].cls);
               prfmsg(SETNOCLS);
          }
          tv_setn(TV_METHOD, method);
          prfmsg(FORCE);
          shocst("GALAV FORCED VALIDATION", "%s validated %s (method %d)",
                 usaptr->userid, uid, method);
          break;

     case 'D':                       /* @D user - devalidate                 */
          if (!needuser(rest, DVALSNTX, uid)) {
               return;
          }
          if (!avu_get(uid, &u)) {
               prfmsg(NOAVREC);
               return;
          }
          u.validated = 0;
          u.attempts = 0;
          u.badcodes = 0;
          u.code = GALAV_NOCODE;
          avu_put(&u);
          prfmsg(DEVAL);
          av_log("SYSOP: %s devalidated %s", usaptr->userid, uid);
          break;

     case 'E':                       /* @E user - email the code again       */
          if (!needuser(rest, EMALSNTX, uid)) {
               return;
          }
          if (!avu_get(uid, &u)) {
               prfmsg(NOAVREC);
               return;
          }
          if (u.email[0] == '\0') {
               prfmsg(NOEMAIL);
               return;
          }
          tv_set(TV_EMAIL, u.email);
          if (av_sendcode(uid)) {
               prfmsg(RESENT);
               av_log("EMAILED: %s resent the code to %s at %s",
                      usaptr->userid, uid, u.email);
          }
          else {
               prfmsg(FSENDBAD);
               av_log("FAILED: %s could not resend the code to %s at %s",
                      usaptr->userid, uid, u.email);
          }
          break;

     case 'L':                       /* @L user - look up                    */
          if (*rest == '\0') {
               prfmsg(LKUPSNTX);
               return;
          }
          sy_userinfo(rest);
          break;

     case 'O':                       /* @O user - toggle the override        */
          if (!needuser(rest, OVERSNTX, uid)) {
               return;
          }
          if (!avu_get(uid, &u)) {
               avu_new(uid, &u);
          }
          u.override = !u.override;
          avu_put(&u);
          tv_set(TV_OVERRIDE, u.override ? "On" : "Off");
          prfmsg(OVERRIDE);
          av_log("SYSOP: %s turned the override %s for %s", usaptr->userid,
                 u.override ? "on" : "off", uid);
          break;

     case '?':
          prfmsg(GLBLCMND);
          break;
     }
}

INT                                  /*   1 = we handled it, 0 = not ours    */
sy_global(VOID)                      /* global command handler               */
{
     CHAR cmd, *rest;

     if (margc < 1 || strlen(margv[0]) != 2 || margv[0][0] != avcfg.globchr) {
          return 0;
     }
     cmd = (CHAR)toupper((UCHAR)margv[0][1]);
     if (strchr("VDELO?", cmd) == NULL || !av_issysop()) {
          return 0;
     }
     setmbk(avmb);
     setglob();
     rstrin();                       /* user-ids may contain spaces          */
     rest = skpwht(margv[0] + 2);    /* margv[0] still points into input */
     unpad(rest);
     gcmd(cmd, rest);
     outprf(usrnum);
     rstmbk();
     return 1;
}
