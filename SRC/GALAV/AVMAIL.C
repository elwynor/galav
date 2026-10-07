/* Build 26.10.07.1 03:36PM */
/*****************************************************************************
 *   AVMAIL.C                           Auto Validator - The validation email *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Builds the validation email - its From address, subject and body - and  *
 *   lets a sysop edit the subject and body online with the BBS editor.      *
 *                                                                           *
 *   From:    the FROMADDR setting.  Blank sends as Sysop, which the SMTP    *
 *            gateway turns into sysop@<its host name>.  A full address is   *
 *            sent as <EMAILPFX><address>: the gateway strips its own prefix *
 *            and uses the rest as the From address unchanged (WG33          *
 *            SMTPUTIL.C:154, SMTPSEND.C:725).                               *
 *   Subject: the EMLSUBJ setting.                                           *
 *   Body:    GALAVEML.TXT in the BBS directory if it exists (written by the *
 *            online editor, plain text), otherwise the YOUVAL message in    *
 *            GALAV.MSG.                                                     *
 *                                                                           *
 *   In the subject and in GALAVEML.TXT, [NAME] stands for the text          *
 *   variable NAME - [GALAV_CODE], [SYSTEM_NAME] and so on - because the     *
 *   editor cannot type the control bytes a real text variable is made of.   *
 *   Only names the BBS actually has registered are replaced, so any other   *
 *   text in brackets is sent as typed.                                      *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "AVSET.H"

#define BODYFILE  "GALAVEML.TXT"     /* the custom body, when there is one   */
#define TVSTART   '\1'               /* a text variable starts and ends with */

/* ------------------------------------------------------------------------ *
 * Text conversion.                                                         *
 * ------------------------------------------------------------------------ */

static VOID
lfix(CHAR *s)                        /* any EOLs -> GME / editor '\r' EOLs   */
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

static VOID
tokens_in(                           /* "[NAME]" -> a real text variable     */
const CHAR *src,
CHAR *dst,
INT size)
{
     CHAR name[TVRSIZ], *end = dst + size - 1;
     INT n;

     while (*src != '\0' && dst < end) {
          if (*src == '[') {
               for (n = 0 ; n < TVRSIZ - 1
                         && (isalnum((UCHAR)src[1 + n]) || src[1 + n] == '_') ; n++) {
                    name[n] = (CHAR)toupper((UCHAR)src[1 + n]);
               }
               name[n] = '\0';
               if (n > 0 && src[1 + n] == ']' && findtvar(name) >= 0
                && dst + n + 4 <= end) {
                    /* 0x01, 'N' = natural width (the width byte is then    */
                    /* ignored), the name, 0x01 - see WG33 MENUING.C grbtxv. */
                    *dst++ = TVSTART;
                    *dst++ = 'N';
                    *dst++ = (CHAR)(32 + 1);
                    strcpy(dst, name);
                    dst += n;
                    *dst++ = TVSTART;
                    src += n + 2;
                    continue;
               }
          }
          if (*src != TVSTART) {     /* a stray 0x01 would garble the rest   */
               *dst++ = *src;
          }
          src++;
     }
     *dst = '\0';
}

static VOID
tokens_out(                          /* a real text variable -> "[NAME]"     */
const CHAR *src,
CHAR *dst,
INT size)
{
     CHAR *end = dst + size - 1;
     const CHAR *close;
     INT n;

     while (*src != '\0' && dst < end) {
          if (*src == TVSTART && src[1] != '\0' && src[2] != '\0'
           && (close = strchr(src + 3, TVSTART)) != NULL) {
               n = (INT)(close - (src + 3));
               if (dst + n + 2 <= end) {
                    *dst++ = '[';
                    memcpy(dst, src + 3, n);
                    dst += n;
                    *dst++ = ']';
               }
               src = close + 1;
               continue;
          }
          if (*src != TVSTART) {
               *dst++ = *src;
          }
          src++;
     }
     *dst = '\0';
}

static VOID
expand(                              /* "[NAME]" text -> final text          */
const CHAR *src,
CHAR *dst,
INT size)
{
     tokens_in(src, dst, size);
     /* Not going through prfmsg(), so no '%' doubling (see AVTVARS.C).      */
     tv_raw(TRUE);
     xlttxv(dst, size);
     tv_raw(FALSE);
     stpans(dst);
}

static VOID
loadbody(                            /* the body, with "[NAME]" variables    */
CHAR *buf,
INT size)
{
     FILE *fp;
     size_t n;

     if ((fp = fopen(BODYFILE, "rb")) != NULL) {
          n = fread(buf, 1, size - 1, fp);
          fclose(fp);
          buf[n] = '\0';
          return;
     }
     tokens_out(rawmsg(YOUVAL), buf, size);
}

/* ------------------------------------------------------------------------ *
 * Composing.  Callers select our message file (setmbk) and set the text    *
 * variables the text may use (GALAV_USERID, GALAV_CODE, GALAV_EMAIL).      *
 * ------------------------------------------------------------------------ */

VOID
ml_compose(                          /* fill in a validation email           */
struct message *msg,                 /*   header to fill in                  */
CHAR *body,                          /*   receives the body ('\r' EOLs)      */
INT size,                            /*   size of body                       */
const CHAR *to)                      /*   the user's address                 */
{
     static CHAR raw[GALAV_BODYSIZ];
     CHAR subj[512];               /* room for variables to expand       */

     setmem(msg, sizeof(*msg), 0);
     if (avcfg.fromadr[0] != '\0') {
          stlcpy(msg->from, avcfg.emlpfx, MAXADR);
          stlcat(msg->from, avcfg.fromadr, MAXADR);
     }
     else {
          stlcpy(msg->from, "Sysop", MAXADR);
     }
     stlcpy(msg->to, avcfg.emlpfx, MAXADR);
     stlcat(msg->to, to, MAXADR);

     /* Never send a mail with no subject.                                  */
     expand(avcfg.emlsubj[0] != '\0' ? avcfg.emlsubj : AVS_DEFSUBJ,
            subj, sizeof(subj));
     strstp(subj, '\r');
     strstp(subj, '\n');
     stzcpy(msg->topic, subj, TPCSIZ);

     loadbody(raw, sizeof(raw));
     expand(raw, body, size);
     lfix(body);
     if (strlen(body) >= TXTLEN) {   /* GME's limit on a message body        */
          body[TXTLEN - 1] = '\0';
     }
}

/* ------------------------------------------------------------------------ *
 * The online editor.  bgnedt() takes the user into the BBS editor (full    *
 * screen, or the line editor for non-ANSI callers) on a topic and a body - *
 * here the subject and the body.  The buffers live in our per-user area,   *
 * which the editor leaves alone, and edone() is called when the sysop      *
 * saves or quits.                                                          *
 * ------------------------------------------------------------------------ */

GBOOL
ml_custom(VOID)                      /* is a custom body in use?             */
{
     return isfile(BODYFILE);
}

static GBOOL
savebody(const CHAR *text)           /* write the edited body to BODYFILE    */
{
     FILE *fp;

     if ((fp = fopen(BODYFILE, "wb")) == NULL) {
          return FALSE;
     }
     for ( ; *text != '\0' ; text++) {
          if (*text == '\r') {
               fputs("\r\n", fp);
          }
          else {
               fputc(*text, fp);
          }
     }
     fputs("\r\n", fp);
     fclose(fp);
     return TRUE;
}

static SHORT
edone(                               /* the editor is finished               */
SHORT flags)                         /*   ED_QUITEX if the sysop quit        */
{
     usrptr->state = avstt;          /* back from the editor to us           */
     setmbk(avmb);
     if (flags & ED_QUITEX) {
          prfmsg(EMLKEPT);
     }
     else {
          unpad(avv->body);
          unpad(avv->subj);
          if (avv->subj[0] != '\0') {
               stzcpy(avcfg.emlsubj, avv->subj, AVS_SUBSIZ);
               set_save();
          }
          if (avv->body[0] == '\0') {          /* empty: use the default    */
               unlink(BODYFILE);
               prfmsg(EMLSAVED);
          }
          else if (savebody(avv->body)) {
               prfmsg(EMLSAVED);
          }
          else {
               prfmsg(EMLWRERR);
          }
          shocst("GALAV EMAIL CHANGED", "%s edited the validation email",
                 usaptr->userid);
          av_log("SETTING: %s edited the validation email", usaptr->userid);
     }
     sy_emlmenu();
     outprf(usrnum);
     rstmbk();
     return 1;                       /* 1: leave the editor                  */
}

VOID
ml_edit(VOID)                        /* open the editor on subject and body  */
{
     CHAR *p;

     setmem(avv->subj, sizeof(avv->subj), 0);
     setmem(avv->body, sizeof(avv->body), 0);
     stzcpy(avv->subj, avcfg.emlsubj, sizeof(avv->subj));
     loadbody(avv->body, sizeof(avv->body));
     lfix(avv->body);
     p = avv->body;                  /* drop leading blank lines             */
     while (*p == '\r') {
          p++;
     }
     movmem(p, avv->body, strlen(p) + 1);
     bgnedt(sizeof(avv->body), avv->body, sizeof(avv->subj), avv->subj,
            edone, 0);
}

VOID
ml_restore(VOID)                     /* default subject and body again       */
{
     unlink(BODYFILE);
     set_default1(AVS_OFS(emlsubj));
}
