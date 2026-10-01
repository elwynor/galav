/* Build 26.10.01.1 03:48PM */
/*****************************************************************************
 *   AVVALID.C                         Auto Validator - Validating a user     *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1995 High Velocity Software, Inc.              *
 *                                                                           *
 *   The user side of the module.  A new user enters, gives an email         *
 *   address, and is emailed a code.  On a later visit they enter the code   *
 *   and are validated: switched to a class, given a key and/or posted       *
 *   credits, according to the validation method (1-5) in the settings.     *
 *                                                                           *
 *   Each routine handles one line of input and returns: TRUE to stay in     *
 *   the module waiting for more input, FALSE when the user is done.  The    *
 *   current step is kept in usrptr->substt (the ST_xxx states, AVDEFS.H).   *
 *   Callers select our message file (setmbk) before calling in, and send    *
 *   the output (outprf) afterwards.                                         *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#define _CRT_RAND_S                  /* rand_s(): unpredictable codes        */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "gme.h"
#include "AVSET.H"
#include "AVUSER.H"

#define BADLIST   "HVSAVBAD.TXT"     /* optional list of refused addresses   */
#define MAXTRIES  3                  /* wrong codes allowed per visit        */
#define BODYSIZ   4000               /* largest validation email body        */

/* ------------------------------------------------------------------------ *
 * Small helpers.                                                           *
 * ------------------------------------------------------------------------ */

static GBOOL
mayvalidate(VOID)                    /* does this user have the request key? */
{
     /* Checked by hand: an empty key means "everyone".                     */
     return avcfg.vldkey[0] == '\0' || haskey(avcfg.vldkey);
}

static GBOOL
yes(VOID)                            /* did the user answer yes?             */
{
     return margc > 0 && toupper((UCHAR)margv[0][0]) == 'Y';
}

static GBOOL
outoftries(const struct avuser *u)   /* has the user used up their attempts? */
{
     return !u->override && avcfg.maxatmpt > 0 && u->attempts >= avcfg.maxatmpt;
}

static SHORT
newcode(VOID)                        /* a fresh, unpredictable code          */
{
     UINT r = 0;

     /* Four hex digits fit the 2.0 record; 0xFFFF is the "no code" marker. */
     rand_s(&r);
     return (SHORT)(r % 0xFFFFU);
}

static const CHAR *
codestr(SHORT code)                  /* a code as the user sees it ("1A2B")  */
{
     static CHAR buf[8];

     sprintf(buf, "%04X", (USHORT)code);
     return buf;
}

static VOID
lfix(CHAR *s)                        /* message text EOLs -> GME '\r' EOLs   */
{
     CHAR *d = s;

     for ( ; *s != '\0' ; s++) {
          if (*s == '\r' && s[1] == '\n') {
               continue;
          }
          *d++ = (*s == '\n') ? '\r' : *s;
     }
     *d = '\0';
}

static GBOOL
inbadlist(const CHAR *addr)          /* is the address refused by the list?  */
{
     FILE *fp;
     CHAR line[80];
     GBOOL hit = FALSE;

     if ((fp = fopen(BADLIST, "r")) == NULL) {
          return FALSE;              /* no list: nothing refused             */
     }
     while (!hit && fgets(line, sizeof(line), fp) != NULL) {
          unpad(line);
          strupr(line);
          hit = (line[0] != '\0' && strstr(addr, line) != NULL);
     }
     fclose(fp);
     return hit;
}

#define ADDR_OK      0
#define ADDR_BADFORM 1
#define ADDR_REFUSED 2

static INT                           /*   ADDR_xxx                           */
chkaddr(                             /* check (and upper-case) an address    */
CHAR *addr)
{
     CHAR *at, *p;

     strupr(addr);
     if (strlen(addr) >= AVU_EMLSIZ || (at = strchr(addr, '@')) == NULL
      || at == addr || strchr(at + 1, '@') != NULL) {
          return ADDR_BADFORM;
     }
     /* The domain needs a dot with something on each side of it.           */
     p = strrchr(at + 1, '.');
     if (p == NULL || p == at + 1 || p[1] == '\0') {
          return ADDR_BADFORM;
     }
     for (p = addr ; *p != '\0' ; p++) {
          if (!isgraph((UCHAR)*p) || strchr("<>()[],;:\\\"%", *p) != NULL) {
               return ADDR_BADFORM;
          }
     }
     if ((avcfg.bademail[0] != '\0' && sameas(at + 1, avcfg.bademail))
      || inbadlist(addr)) {
          return ADDR_REFUSED;
     }
     return ADDR_OK;
}

/* ------------------------------------------------------------------------ *
 * Sending the code.                                                        *
 * ------------------------------------------------------------------------ */

GBOOL                                /*   TRUE if accepted for delivery      */
av_sendcode(                         /* email a user their validation code   */
const CHAR *userid)                  /*   whose code (issues one if needed)  */
{
     struct avuser u;
     static struct message msg;
     static CHAR body[BODYSIZ];

     if (!avu_get(userid, &u) || u.email[0] == '\0') {
          return FALSE;
     }
     if (u.code == HVS_NOCODE) {
          u.code = newcode();
          u.method = avcfg.emlmeth;
          avu_put(&u);
     }

     setmem(&msg, sizeof(msg), 0);
     stlcpy(msg.from, "Sysop", MAXADR);
     stlcpy(msg.to, avcfg.emlpfx, MAXADR);
     stlcat(msg.to, u.email, MAXADR);
     stlcpy(msg.topic, rawmsg(EMLSUBJ), TPCSIZ);
     stpans(msg.topic);
     strstp(msg.topic, '\r');
     strstp(msg.topic, '\n');

     /* Build the body from the YOUVAL text.  It never goes through prfmsg, */
     /* so expand its text variables directly with xlttxv() and turn off    */
     /* the '%' doubling those variables normally do (see AVTVARS.C).        */
     tv_set(TV_USERID, u.userid);
     tv_set(TV_CODE, codestr(u.code));
     stzcpy(body, rawmsg(YOUVAL), BODYSIZ);
     tv_raw(TRUE);
     xlttxv(body, BODYSIZ);
     tv_raw(FALSE);
     stpans(body);
     lfix(body);

     /* simpsnd() never waits: GMEAGAIN means GME queued the message and    */
     /* will finish sending it in the background.  Both count as success.   */
     return simpsnd(&msg, body, NULL) >= GMEAGAIN;
}

/* ------------------------------------------------------------------------ *
 * Validating.                                                              *
 * ------------------------------------------------------------------------ */

static VOID
switchclass(                         /* move a user to a new class           */
const CHAR *userid,
const CHAR *clsnam)
{
     static struct usracc acc;       /* offline account (too big for stack)  */

     /* Pass swtcls() the account the engine is really using: the caller's */
     /* own if it is them, the live copy if they are online (onbbs() finds  */
     /* users even part-way through logon), or else the stored account.     */
     if (sameas(userid, usaptr->userid)) {
          swtcls(usaptr, 1, clsnam, 4, 0);
     }
     else if (onbbs(userid, 1)) {
          swtcls(uacoff(uisusn), 1, clsnam, 4, 0);
     }
     else {
          GBOOL found;

          dfaSetBlk(accbb);
          found = dfaAcqEQ(&acc, userid, 0);
          dfaRstBlk();
          if (found) {
               swtcls(&acc, 1, clsnam, 4, 0);
          }
     }
}

GBOOL                                /*   FALSE if the class was missing     */
av_validate(                         /* validate a user with a method        */
const CHAR *userid,                  /*   user to validate                   */
INT method,                          /*   method 1-5                         */
GBOOL forced)                        /*   TRUE: by a sysop (no user output)  */
{
     struct avmeth *m = &avcfg.meth[method - 1];
     struct avuser u;
     CHAR uid[UIDSIZ], amount[16], keylist[KEYSIZ + 1];
     GBOOL ok = TRUE;

     stzcpy(uid, userid, UIDSIZ);
     if (!avu_get(uid, &u)) {
          avu_new(uid, &u);
     }

     if (m->creds > 0) {
          sprintf(amount, "%ld", m->creds);
          addcrd(uid, amount, m->paid);
     }

     if (m->cls[0] != '\0') {
          /* Never switch to a missing class - the engine would delete the  */
          /* account.  Skip the switch and tell the sysop instead.          */
          if (fndcls(m->cls) == NULL) {
               ok = FALSE;
               shocst("HVSAV CLASS MISSING",
                      "Method %d class %s not found; %s not switched",
                      method, m->cls, uid);
               av_log("ERROR: method %d class %s does not exist; %s kept "
                      "their class", method, m->cls, uid);
          }
          else {
               switchclass(uid, m->cls);
          }
     }

     if (m->key[0] != '\0') {
          stzcpy(keylist, m->key, sizeof(keylist));
          givkey(uid, keylist);
     }

     avu_get(uid, &u);
     u.validated = 1;
     u.method = (SHORT)method;
     avu_put(&u);

     if (!forced) {
          if (!ok) {
               prfmsg(VALIDERR);
          }
          prfmsg(LATER);
          if (m->creds > 0) {
               tv_setn(TV_CREDITS, m->creds);
               prfmsg(ADDCRDT);
          }
     }
     av_log("SUCCESS: %s validated with method %d%s", uid, method,
            forced ? spr(" by sysop %s", usaptr->userid) : "");
     return ok;
}

/* ------------------------------------------------------------------------ *
 * The dialog.                                                              *
 * ------------------------------------------------------------------------ */

GBOOL
av_begin(                            /* user arrives: decide what to do      */
GBOOL fromlogon)                     /*   TRUE: came from the logon offer    */
{
     struct avuser u;

     (VOID)fromlogon;
     avv->tries = 0;
     if (!mayvalidate()) {
          prfmsg(ALRDYVLD);
          return FALSE;
     }
     if (!avu_get(usaptr->userid, &u)) {
          avu_new(usaptr->userid, &u);
     }
     if (u.validated) {
          prfmsg(POSTVALD);
          return FALSE;
     }
     if (outoftries(&u)) {
          prfmsg(TOMNYVLD);
          av_log("SCREEN: %s has used all validation attempts", u.userid);
          return FALSE;
     }
     if (u.code != HVS_NOCODE) {     /* code already sent: ask for it        */
          tv_set(TV_EMAIL, u.email);
          prfmsg(MAILQURY);
          usrptr->substt = ST_CODE;
          return TRUE;
     }
     tv_set(TV_EMAILTYP, avcfg.emltype);
     prfmsg(EMALCONT);
     usrptr->substt = ST_EMLCONT;
     return TRUE;
}

static GBOOL
gotaddress(VOID)                     /* ST_EMLADDR: an address was typed     */
{
     struct avuser u;
     CHAR addr[INPSIZ];

     rstrin();                       /* whole line, as typed                 */
     stzcpy(addr, skpwht(input), sizeof(addr));
     unpad(addr);
     if (addr[0] == '\0') {
          prfmsg(EMALQURY);
          return TRUE;
     }
     if (sameas(addr, "X")) {
          prfmsg(COMEBACK);
          return FALSE;
     }
     switch (chkaddr(addr)) {
     case ADDR_BADFORM:
          prfmsg(OOPSQURY);
          return TRUE;
     case ADDR_REFUSED:
          av_log("SCREEN: %s offered refused address %s", usaptr->userid, addr);
          prfmsg(EMAILNO);
          return TRUE;
     }

     avu_get(usaptr->userid, &u);
     if (!u.override && avu_emailcount(addr, u.userid) >= avcfg.maxemail) {
          tv_set(TV_EMAILTYP, avcfg.emltype);
          prfmsg(CNTEMAIL);
          av_log("SCREEN: %s - address %s already used by too many users",
                 u.userid, addr);
          return FALSE;
     }

     stzcpy(u.email, addr, AVU_EMLSIZ);
     u.code = HVS_NOCODE;            /* av_sendcode() issues a fresh one     */
     avu_put(&u);
     tv_set(TV_EMAIL, u.email);
     if (av_sendcode(u.userid)) {
          prfmsg(MAILSENT);
          av_log("EMAILED: code sent to %s at %s", u.userid, u.email);
     }
     else {
          /* Drop the code so their next visit offers email again rather    */
          /* than asking for a code that never arrived.                      */
          avu_get(u.userid, &u);
          u.code = HVS_NOCODE;
          avu_put(&u);
          prfmsg(SENDFAIL);
          shocst("HVSAV EMAIL FAILED", "Could not send code to %s", u.userid);
          av_log("FAILED: could not send code to %s at %s", u.userid, u.email);
     }
     return FALSE;
}

static GBOOL
gotcode(VOID)                        /* ST_CODE: a code was typed            */
{
     struct avuser u;
     INT method;

     if (!avu_get(usaptr->userid, &u)) {
          return FALSE;
     }
     tv_set(TV_EMAIL, u.email);
     if (margc == 0) {
          prfmsg(MAILQURY);
          return TRUE;
     }
     if (sameas(margv[0], "X")) {
          prfmsg(ABORTCDE);
          return FALSE;
     }
     if (u.code != HVS_NOCODE && sameas(margv[0], codestr(u.code))) {
          /* Others may have validated with this address since the code was */
          /* sent, so check the shared-address limit again.                  */
          if (!u.override
           && avu_emailcount(u.email, u.userid) >= avcfg.maxemail) {
               tv_set(TV_EMAILTYP, avcfg.emltype);
               prfmsg(CNTEMAIL);
               av_log("SCREEN: %s - address %s already used by too many "
                      "users", u.userid, u.email);
               return FALSE;
          }
          /* Apply the method recorded when the code was issued; codes      */
          /* issued by 2.0 for postal mail carry their own method too.      */
          method = (u.method >= 1 && u.method <= HVS_NMETH) ? u.method
                                                            : avcfg.emlmeth;
          av_validate(u.userid, method, FALSE);
          return FALSE;
     }
     if (++avv->tries < MAXTRIES) {
          prfmsg(MISTYPE);
          return TRUE;
     }
     prfmsg(SORRY);
     u.attempts++;
     avu_put(&u);
     av_log("FAILED: %s entered the wrong code %d times (attempt %d)",
            u.userid, MAXTRIES, u.attempts);
     return FALSE;
}

GBOOL
av_input(VOID)                       /* one line of input in the dialog      */
{
     switch (usrptr->substt) {
     case ST_LOGONQ:
          return yes() ? av_begin(TRUE) : FALSE;
     case ST_EMLCONT:
          if (yes()) {
               prfmsg(EMALQURY);
               usrptr->substt = ST_EMLADDR;
               return TRUE;
          }
          prfmsg(COMEBACK);
          return FALSE;
     case ST_EMLADDR:
          return gotaddress();
     case ST_CODE:
          return gotcode();
     }
     return FALSE;
}

GBOOL
av_logon(VOID)                       /* logon: offer validation if useful    */
{
     struct avuser u;

     if (!avcfg.asklogon || !mayvalidate() || av_issysop()) {
          return FALSE;
     }
     if (avu_get(usaptr->userid, &u) && (u.validated || outoftries(&u))) {
          return FALSE;
     }
     prfmsg(AUTOQURY);
     usrptr->substt = ST_LOGONQ;
     return TRUE;
}
