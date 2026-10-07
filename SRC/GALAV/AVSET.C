/* Build 26.10.07.1 03:36PM */
/*****************************************************************************
 *   AVSET.C                                Auto Validator - Live settings    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Loads, seeds and saves the settings record (GALAVSET.DAT), and          *
 *   describes each setting - its label, its allowed values, where it lives  *
 *   in the record and its default - so the sysop editor in AVSYSOP.C can    *
 *   list and change them generically.                                       *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "AVSET.H"

struct avsetdat avcfg;               /* the live settings                    */
static DFAFILE *avsdat;              /* GALAVSET.DAT                         */

/* ------------------------------------------------------------------------ *
 * Setting types.                                                           *
 * ------------------------------------------------------------------------ */
enum { T_KEY, T_CLS, T_STR, T_NUM, T_LNG, T_BOOL, T_CHR, T_PAY, T_ADR, T_FIL };

struct setdef {
     INT type;                       /* T_xxx                                */
     size_t offset;                  /* where it lives in struct avsetdat    */
     INT size;                       /* string buffer size (incl. NUL)       */
     LONG min, max;                  /* numeric range                        */
     LONG defnum;                    /* default: numbers, YES=1, PAID=1      */
     const CHAR *defstr;             /* default: text settings and T_CHR     */
     const CHAR *label;              /* what the sysop sees                  */
};

/* The defaults live here, not in GALAV.MSG: settings are changed only     */
/* online, so there is one place to change them.  They are used on a new   */
/* install and by "Reset all settings" in the sysop menu.                  */
#define NUM(t, f, lo, hi, def, lbl) { t, AVS_OFS(f), 0, lo, hi, def, NULL, lbl }
#define STR(t, f, sz, def, lbl)     { t, AVS_OFS(f), sz, 0, 0, 0, def, lbl }

/* Page 0: general settings, in the order the sysop sees them.              */
static const struct setdef gendefs[] = {
     STR(T_KEY,  vldkey,   KEYSIZ,      "",          "Key needed to request validation"),
     STR(T_KEY,  cfgkey,   KEYSIZ,      "SYSOP",     "Key for sysop menu and commands"),
     NUM(T_NUM,  emlmeth,  1, GALAV_NMETH, 1,        "Method used for email validations"),
     NUM(T_NUM,  maxemail, 1, 100, 1,                "Max validated users per email"),
     NUM(T_NUM,  maxatmpt, 0, 100, 3,                "Failed validations allowed (0=any)"),
     NUM(T_BOOL, asklogon, 0, 0, 0,                  "Offer validation at logon"),
     STR(T_CHR,  globchr,  0,           "@",         "Global command trigger"),
     STR(T_STR,  emltype,  AVS_TYPSIZ,  "Internet",  "Email type shown to users"),
     STR(T_STR,  emlpfx,   AVS_PFXSIZ,  "IN:",       "Internet email address prefix"),
     STR(T_ADR,  fromadr,  AVS_FROMSIZ, "",          "Validation email sent from"),
     STR(T_STR,  emlsubj,  AVS_SUBSIZ,  AVS_DEFSUBJ, "Validation email subject"),
     STR(T_STR,  bademail, AVS_BADSIZ,  "",          "Email domain refused"),
     STR(T_FIL,  logfile,  AVS_LOGSIZ,  "GALAV.LOG", "Activity log file (blank=off)"),
};
#define NGEN (sizeof(gendefs) / sizeof(gendefs[0]))

/* Pages 1-5: the same four settings for each validation method.  Offsets   */
/* here are within struct avmeth; method n's copy is found at run time.     */
/* Every method has the same defaults: no class, no key, no credits, FREE.  */
#define MOFS(f) offsetof(struct avmeth, f)
static const struct setdef methdefs[] = {
     { T_CLS, MOFS(cls),   KEYSIZ, 0, 0,        0, "", "Class to switch to" },
     { T_KEY, MOFS(key),   KEYSIZ, 0, 0,        0, "", "Key to give" },
     { T_LNG, MOFS(creds), 0,      0, 1000000L, 0, NULL, "Credits to post" },
     { T_PAY, MOFS(paid),  0,      0, 0,        0, NULL, "Credit type" },
};
#define NMDEF (sizeof(methdefs) / sizeof(methdefs[0]))
#define NSET  (NGEN + GALAV_NMETH * NMDEF)

/* Look up setting idx (0..NSET-1): its definition and its storage.         */
static const struct setdef *
setdef(INT idx, VOID **where)
{
     const struct setdef *d;

     if (idx < (INT)NGEN) {
          d = &gendefs[idx];
          *where = (CHAR *)&avcfg + d->offset;
     }
     else {
          INT m = (idx - (INT)NGEN) / (INT)NMDEF;   /* method 0-4            */
          INT f = (idx - (INT)NGEN) % (INT)NMDEF;   /* field within method   */

          d = &methdefs[f];
          *where = (CHAR *)&avcfg.meth[m] + d->offset;
     }
     return d;
}

/* ------------------------------------------------------------------------ *
 * File handling.                                                           *
 * ------------------------------------------------------------------------ */

VOID
set_save(VOID)                       /* write avcfg to GALAVSET.DAT          */
{
     struct avsetrec rec;

     /* rec.recid is the lookup key: it must be writable memory (see      */
     /* set_open).                                                         */
     setmem(&rec, sizeof(rec), 0);
     stzcpy(rec.recid, AVS_RECID, sizeof(rec.recid));
     avcfg.layout = AVS_LAYOUT;
     rec.d = avcfg;
     dfaSetBlk(avsdat);
     if (dfaAcqEQ(NULL, rec.recid, 0)) {
          dfaUpdate(&rec);
     }
     else {
          dfaInsert(&rec);
     }
     dfaRstBlk();
}

static GBOOL
goodaddr(const CHAR *s)              /* is s a plausible name@domain.tld?    */
{
     const CHAR *at = strchr(s, '@'), *dot;

     if (at == NULL || at == s || strchr(at + 1, '@') != NULL) {
          return FALSE;
     }
     dot = strrchr(at + 1, '.');
     if (dot == NULL || dot == at + 1 || dot[1] == '\0') {
          return FALSE;
     }
     for ( ; *s != '\0' ; s++) {
          if (!isgraph((UCHAR)*s) || strchr("<>()[],;:\\\"%", *s) != NULL) {
               return FALSE;
          }
     }
     return TRUE;
}

static GBOOL
plainname(const CHAR *s)             /* a file name with no drive or folder? */
{
     /* The sysop key may be given to co-sysops, so the online editor must */
     /* not let it name an arbitrary file to append the log to.             */
     if (*s == '\0' || *s == '.') {
          return FALSE;
     }
     for ( ; *s != '\0' ; s++) {
          if (!isgraph((UCHAR)*s) || strchr("\\/:*?\"<>|", *s) != NULL) {
               return FALSE;
          }
     }
     return TRUE;
}

static VOID
set_sanity(VOID)                     /* keep loaded values inside limits     */
{
     INT i;
     VOID *where;

     for (i = 0 ; i < (INT)NSET ; i++) {
          const struct setdef *d = setdef(i, &where);

          switch (d->type) {
          case T_NUM:
               if (*(SHORT *)where < d->min || *(SHORT *)where > d->max) {
                    *(SHORT *)where = (SHORT)d->min;
               }
               break;
          case T_LNG:
               if (*(LONG *)where < d->min || *(LONG *)where > d->max) {
                    *(LONG *)where = d->min;
               }
               break;
          case T_BOOL:
          case T_PAY:
               *(SHORT *)where = (*(SHORT *)where != 0);
               break;
          case T_CHR:
               if (!isgraph((UCHAR)*(CHAR *)where)) {
                    *(CHAR *)where = '@';
               }
               break;
          case T_ADR:
               /* It goes into the mail header: never trust it unchecked.   */
               ((CHAR *)where)[d->size - 1] = '\0';
               if (*(CHAR *)where != '\0' && !goodaddr((CHAR *)where)) {
                    *(CHAR *)where = '\0';
               }
               break;
          default:                   /* strings: guarantee termination       */
               ((CHAR *)where)[d->size - 1] = '\0';
               break;
          }
     }
}

static VOID
loadone(INT idx)                     /* copy one default into avcfg          */
{
     VOID *where;
     const struct setdef *d = setdef(idx, &where);

     switch (d->type) {
     case T_NUM:
     case T_BOOL:
     case T_PAY:
          *(SHORT *)where = (SHORT)d->defnum;
          break;
     case T_LNG:
          *(LONG *)where = d->defnum;
          break;
     case T_CHR:
          *(CHAR *)where = d->defstr[0];
          break;
     default:                        /* T_KEY, T_CLS, T_STR, T_ADR, T_FIL    */
          stzcpy((CHAR *)where, d->defstr, d->size);
          break;
     }
}

VOID
set_defaults(VOID)                   /* every setting to its default, saved  */
{
     INT i;

     setmem(&avcfg, sizeof(avcfg), 0);
     for (i = 0 ; i < (INT)NSET ; i++) {
          loadone(i);
     }
     set_sanity();
     set_save();
}

VOID
set_default1(                        /* one setting to its default, saved    */
size_t offset)                       /*   AVS_OFS(field) (general page only) */
{
     INT i;

     for (i = 0 ; i < (INT)NGEN ; i++) {
          if (gendefs[i].offset == offset) {
               loadone(i);
          }
     }
     set_sanity();
     set_save();
}

VOID
set_open(VOID)                       /* open file; load settings or seed them*/
{
     struct avsetrec rec;

     if (!isfile(AVS_FILE)) {
          struct dfaSegSpec seg;
          struct dfaKeySpec key;

          setmem(&seg, sizeof(seg), 0);
          seg.position = offsetof(struct avsetrec, recid);
          seg.length = sizeof(rec.recid);
          seg.type = DFAST_ZSTRING;
          setmem(&key, sizeof(key), 0);
          key.flags = 0;
          key.nSegments = 1;
          key.segs = &seg;
          dfaCreateSpec(AVS_FILE, FALSE, sizeof(struct avsetrec), 1024,
                        0, 0, 1, &key, NULL);
     }
     avsdat = dfaOpen(AVS_FILE, sizeof(struct avsetrec), NULL);

     /* The key must be in writable memory.  dfaAcqEQ copies only as many   */
     /* key bytes as goodblk() allows, and goodblk() returns 0 for          */
     /* read-only memory (WG33 ISGOODPT.C qryBlockSize, DFAAPI.C           */
     /* dfaAcqLock).  A string constant is read-only, so passing AVS_RECID  */
     /* searched for an empty key and reseeded the settings every start     */
     /* (2.1.0 and 2.2.0).                                                  */
     setmem(&rec, sizeof(rec), 0);
     stzcpy(rec.recid, AVS_RECID, sizeof(rec.recid));
     dfaSetBlk(avsdat);
     if (dfaAcqEQ(&rec, rec.recid, 0)) {
          dfaRstBlk();
          avcfg = rec.d;
          set_sanity();
          if (avcfg.layout < 2) {    /* a 2.1.0 record: seed the new fields  */
               set_default1(AVS_OFS(fromadr));
               set_default1(AVS_OFS(emlsubj));
          }
          return;
     }
     dfaRstBlk();
     set_defaults();                 /* a new install: the defaults          */
     shocst("GALAV SETTINGS CREATED",
            "New settings file, default values");
}

VOID
set_close(VOID)
{
     dfaClose(avsdat);
}

/* ------------------------------------------------------------------------ *
 * Editor support.                                                          *
 * ------------------------------------------------------------------------ */

INT
set_count(INT page)
{
     return page == SETPG_GENERAL ? (INT)NGEN : (INT)NMDEF;
}

INT                                  /*   table index, or -1 if out of range */
set_index(INT page, INT n)           /* item n (1-based) on a page           */
{
     if (n < 1 || n > set_count(page)) {
          return -1;
     }
     if (page == SETPG_GENERAL) {
          return n - 1;
     }
     return (INT)NGEN + (page - 1) * (INT)NMDEF + (n - 1);
}

const CHAR *
set_title(INT page)
{
     static CHAR buf[48];

     if (page == SETPG_GENERAL) {
          return "General Settings";
     }
     sprintf(buf, "Validation Method %d Settings", page);
     return buf;
}

const CHAR *
set_label(INT idx)
{
     VOID *where;

     return setdef(idx, &where)->label;
}

const CHAR *
set_value(INT idx)                   /* current value, ready for display     */
{
     static CHAR buf[64];
     VOID *where;
     const struct setdef *d = setdef(idx, &where);

     switch (d->type) {
     case T_NUM:
          sprintf(buf, "%d", *(SHORT *)where);
          break;
     case T_LNG:
          sprintf(buf, "%ld", *(LONG *)where);
          break;
     case T_BOOL:
          strcpy(buf, *(SHORT *)where ? "YES" : "NO");
          break;
     case T_PAY:
          strcpy(buf, *(SHORT *)where ? "PAID" : "FREE");
          break;
     case T_CHR:
          sprintf(buf, "%c", *(CHAR *)where);
          break;
     default:
          if (*(CHAR *)where == '\0') {
               strcpy(buf, d->type == T_ADR ? "(default)" : "(none)");
          }
          else {
               stzcpy(buf, (CHAR *)where, sizeof(buf));
          }
          break;
     }
     return buf;
}

const CHAR *
set_limit(INT idx)                   /* describe the allowed values          */
{
     static CHAR buf[80];
     VOID *where;
     const struct setdef *d = setdef(idx, &where);

     switch (d->type) {
     case T_NUM:
     case T_LNG:
          sprintf(buf, "a number from %ld to %ld", d->min, d->max);
          break;
     case T_BOOL:
          strcpy(buf, "YES or NO");
          break;
     case T_PAY:
          strcpy(buf, "FREE or PAID");
          break;
     case T_CHR:
          strcpy(buf, "one character, not a space");
          break;
     case T_KEY:
          sprintf(buf, "a key name, up to %d characters, no spaces", d->size - 1);
          break;
     case T_CLS:
          sprintf(buf, "an existing class name, up to %d characters", d->size - 1);
          break;
     case T_ADR:
          sprintf(buf, "a full address (name@example.com), up to %d characters",
                  d->size - 1);
          break;
     case T_FIL:
          sprintf(buf, "a file name in the BBS directory, up to %d characters",
                  d->size - 1);
          break;
     default:
          sprintf(buf, "text, up to %d characters", d->size - 1);
          break;
     }
     return buf;
}

static GBOOL
noblanks(const CHAR *s)              /* TRUE if s has no spaces              */
{
     for ( ; *s != '\0' ; s++) {
          if (isspace((UCHAR)*s)) {
               return FALSE;
          }
     }
     return TRUE;
}

INT                                  /*   SETCHG_xxx                         */
set_change(                          /* apply a value typed by the sysop     */
INT idx,                             /*   setting (table index)              */
const CHAR *text)                    /*   what was typed, already trimmed    */
{
     VOID *where;
     LONG n;
     CHAR *end;
     const struct setdef *d = setdef(idx, &where);
     GBOOL clear = sameas(text, "-");

     if (text[0] == '\0') {
          return SETCHG_SAME;
     }
     switch (d->type) {
     case T_NUM:
     case T_LNG:
          n = strtol(text, &end, 10);
          if (*end != '\0' || n < d->min || n > d->max) {
               return SETCHG_BAD;
          }
          if (d->type == T_NUM) {
               *(SHORT *)where = (SHORT)n;
          }
          else {
               *(LONG *)where = n;
          }
          break;
     case T_BOOL:
          if (sameas(text, "Y") || sameas(text, "YES")) {
               *(SHORT *)where = 1;
          }
          else if (sameas(text, "N") || sameas(text, "NO")) {
               *(SHORT *)where = 0;
          }
          else {
               return SETCHG_BAD;
          }
          break;
     case T_PAY:
          if (sameas(text, "FREE")) {
               *(SHORT *)where = 0;
          }
          else if (sameas(text, "PAID")) {
               *(SHORT *)where = 1;
          }
          else {
               return SETCHG_BAD;
          }
          break;
     case T_CHR:
          if (strlen(text) != 1 || !isgraph((UCHAR)text[0])) {
               return SETCHG_BAD;
          }
          *(CHAR *)where = text[0];
          break;
     case T_CLS:
          /* A missing class matters: switching a user to a class that does */
          /* not exist makes the engine delete the account.  Refuse it here, */
          /* and AVVALID.C checks again before every switch.                 */
          if (!clear) {
               if ((INT)strlen(text) >= d->size || !noblanks(text)) {
                    return SETCHG_BAD;
               }
               if (fndcls(text) == NULL) {
                    return SETCHG_NOCLS;
               }
          }
          stzcpy((CHAR *)where, clear ? "" : text, d->size);
          break;
     case T_KEY:
          if (!clear && ((INT)strlen(text) >= d->size || !noblanks(text))) {
               return SETCHG_BAD;
          }
          stzcpy((CHAR *)where, clear ? "" : text, d->size);
          break;
     case T_FIL:
          if (!clear && ((INT)strlen(text) >= d->size || !plainname(text))) {
               return SETCHG_BAD;
          }
          stzcpy((CHAR *)where, clear ? "" : text, d->size);
          break;
     case T_ADR:
          /* Blank means the gateway's own default (Sysop at the SMTP host). */
          if (!clear && ((INT)strlen(text) >= d->size || !goodaddr(text))) {
               return SETCHG_BAD;
          }
          stzcpy((CHAR *)where, clear ? "" : text, d->size);
          break;
     default:                        /* T_STR                                */
          if (!clear && (INT)strlen(text) >= d->size) {
               return SETCHG_BAD;
          }
          stzcpy((CHAR *)where, clear ? "" : text, d->size);
          break;
     }
     set_save();
     return SETCHG_OK;
}
