/* Build 26.10.01.1 03:48PM */
/*****************************************************************************
 *   AVSET.C                                Auto Validator - Live settings    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Loads, seeds and saves the settings record (HVSAVSET.DAT), and          *
 *   describes each setting - its label, its allowed values, where it lives  *
 *   in the record, and which CNF option supplies its install default - so   *
 *   the sysop editor in AVSYSOP.C can list and change them generically.     *
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
static DFAFILE *avsdat;              /* HVSAVSET.DAT                         */

/* ------------------------------------------------------------------------ *
 * Setting types.                                                           *
 * ------------------------------------------------------------------------ */
enum { T_KEY, T_CLS, T_STR, T_NUM, T_LNG, T_BOOL, T_CHR, T_PAY };

struct setdef {
     INT type;                       /* T_xxx                                */
     INT msgnum;                     /* CNF option holding the default       */
     size_t offset;                  /* where it lives in struct avsetdat    */
     INT size;                       /* string buffer size (incl. NUL)       */
     LONG min, max;                  /* numeric range                        */
     const CHAR *label;              /* what the sysop sees                  */
};

#define OFS(f) offsetof(struct avsetdat, f)

/* Page 0: general settings, in the order the sysop sees them.              */
static const struct setdef gendefs[] = {
     { T_KEY,  VALIDKEY, OFS(vldkey),   KEYSIZ,     0,   0, "Key needed to request validation" },
     { T_KEY,  CNFGACES, OFS(cfgkey),   KEYSIZ,     0,   0, "Key for sysop menu and commands" },
     { T_NUM,  EMAILMTD, OFS(emlmeth),  0,          1,   HVS_NMETH, "Method used for email validations" },
     { T_NUM,  MAXEMAIL, OFS(maxemail), 0,          1, 100, "Max validated users per email" },
     { T_NUM,  MAXATMPT, OFS(maxatmpt), 0,          0, 100, "Failed validations allowed (0=any)" },
     { T_BOOL, AVATLOGN, OFS(asklogon), 0,          0,   0, "Offer validation at logon" },
     { T_CHR,  AVGLOB,   OFS(globchr),  0,          0,   0, "Global command trigger" },
     { T_STR,  EMAILTYP, OFS(emltype),  AVS_TYPSIZ, 0,   0, "Email type shown to users" },
     { T_STR,  EMAILPFX, OFS(emlpfx),   AVS_PFXSIZ, 0,   0, "Internet email address prefix" },
     { T_STR,  BADEMAIL, OFS(bademail), AVS_BADSIZ, 0,   0, "Email domain refused" },
     { T_STR,  LOGFILE,  OFS(logfile),  AVS_LOGSIZ, 0,   0, "Activity log file (blank=off)" },
};
#define NGEN (sizeof(gendefs) / sizeof(gendefs[0]))

/* Pages 1-5: the same four settings for each validation method.  Offsets   */
/* here are within struct avmeth; method n's copy is found at run time.     */
#define MOFS(f) offsetof(struct avmeth, f)
static const struct setdef methdefs[] = {
     { T_CLS, MTD1CLS,  MOFS(cls),   KEYSIZ, 0, 0,        "Class to switch to" },
     { T_KEY, MTD1KEY,  MOFS(key),   KEYSIZ, 0, 0,        "Key to give" },
     { T_LNG, MTD1CRED, MOFS(creds), 0,      0, 1000000L, "Credits to post" },
     { T_PAY, MTD1PAYM, MOFS(paid),  0,      0, 0,        "Credit type" },
};
#define NMDEF (sizeof(methdefs) / sizeof(methdefs[0]))
#define NSET  (NGEN + HVS_NMETH * NMDEF)

/* Look up setting idx (0..NSET-1): its definition, its storage, and the    */
/* CNF option it defaults from.                                             */
static const struct setdef *
setdef(INT idx, VOID **where, INT *msgnum)
{
     const struct setdef *d;

     if (idx < (INT)NGEN) {
          d = &gendefs[idx];
          *where = (CHAR *)&avcfg + d->offset;
          *msgnum = d->msgnum;
     }
     else {
          INT m = (idx - (INT)NGEN) / (INT)NMDEF;   /* method 0-4            */
          INT f = (idx - (INT)NGEN) % (INT)NMDEF;   /* field within method   */

          d = &methdefs[f];
          *where = (CHAR *)&avcfg.meth[m] + d->offset;
          /* MTDnCLS, KEY, CRED, PAYM are consecutive in HVSAV.MSG.         */
          *msgnum = d->msgnum + m * (INT)NMDEF;
     }
     return d;
}

/* ------------------------------------------------------------------------ *
 * File handling.                                                           *
 * ------------------------------------------------------------------------ */

static VOID
set_save(VOID)                       /* write avcfg to HVSAVSET.DAT          */
{
     struct avsetrec rec;

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

static VOID
set_sanity(VOID)                     /* keep loaded values inside limits     */
{
     INT i, msgnum;
     VOID *where;

     for (i = 0 ; i < (INT)NSET ; i++) {
          const struct setdef *d = setdef(i, &where, &msgnum);

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
          default:                   /* strings: guarantee termination       */
               ((CHAR *)where)[d->size - 1] = '\0';
               break;
          }
     }
}

VOID
set_defaults(VOID)                   /* copy the CNF options in, and save    */
{
     INT i, msgnum;
     VOID *where;
     CHAR *s;

     setmbk(avmb);
     setmem(&avcfg, sizeof(avcfg), 0);
     for (i = 0 ; i < (INT)NSET ; i++) {
          const struct setdef *d = setdef(i, &where, &msgnum);

          switch (d->type) {
          case T_NUM:
               *(SHORT *)where = (SHORT)numopt(msgnum, (INT)d->min, (INT)d->max);
               break;
          case T_LNG:
               *(LONG *)where = lngopt(msgnum, d->min, d->max);
               break;
          case T_BOOL:
               *(SHORT *)where = (SHORT)(ynopt(msgnum) != 0);
               break;
          case T_CHR:
               *(CHAR *)where = (CHAR)chropt(msgnum);
               break;
          case T_PAY:
               *(SHORT *)where = (SHORT)(tokopt(msgnum, "FREE", "PAID", NULL) == 2);
               break;
          default:                   /* T_KEY, T_CLS, T_STR                  */
               s = stgopt(msgnum);
               stzcpy((CHAR *)where, s, d->size);
               free(s);
               break;
          }
     }
     rstmbk();
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

     setmem(&rec, sizeof(rec), 0);
     dfaSetBlk(avsdat);
     if (dfaAcqEQ(&rec, AVS_RECID, 0)) {
          dfaRstBlk();
          avcfg = rec.d;
          set_sanity();
          return;
     }
     dfaRstBlk();
     set_defaults();                 /* first start: seed from HVSAV.MSG     */
     shocst("HVSAV SETTINGS CREATED",
            "Auto Validator settings seeded from HVSAV.MSG defaults");
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
     INT msgnum;

     return setdef(idx, &where, &msgnum)->label;
}

const CHAR *
set_value(INT idx)                   /* current value, ready for display     */
{
     static CHAR buf[64];
     VOID *where;
     INT msgnum;
     const struct setdef *d = setdef(idx, &where, &msgnum);

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
               strcpy(buf, "(none)");
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
     INT msgnum;
     const struct setdef *d = setdef(idx, &where, &msgnum);

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
     INT msgnum;
     LONG n;
     CHAR *end;
     const struct setdef *d = setdef(idx, &where, &msgnum);
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
