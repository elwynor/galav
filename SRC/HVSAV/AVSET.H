/* Build 26.10.01.2 04:18PM */
/*****************************************************************************
 *   AVSET.H                                Auto Validator - Live settings    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   The settings the module runs on live in HVSAVSET.DAT, not in the CNF    *
 *   options.  On first start the CNF options (HVSAV.MSG) are copied in as   *
 *   install defaults; after that the sysop changes settings online from     *
 *   the sysop menu and every change applies immediately.  Code reads the    *
 *   in-memory copy (avcfg) at the point of use - never a value cached at    *
 *   startup.                                                                *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#ifndef AVSET_H
#define AVSET_H

#include "AVDEFS.H"

#define AVS_FILE     "HVSAVSET.DAT"
#define AVS_RECID    "SETTINGS"      /* key of the one settings record       */
/* 512 bytes: the settings use about 350.  The record must fit a 1024-byte */
/* Btrieve page together with Btrieve's own overhead - a 1024-byte record  */
/* fails with status 28 (invalid record length).                            */
#define AVS_RECLEN   512             /* fixed record length, room to grow    */
#define AVS_LAYOUT   1               /* bump when the record layout changes  */

#define AVS_TYPSIZ   21              /* EMAILTYP  (S 20)                     */
#define AVS_PFXSIZ   6               /* EMAILPFX  (S 5)                      */
#define AVS_BADSIZ   38              /* BADEMAIL  (S 37)                     */
#define AVS_LOGSIZ   41              /* LOGFILE   (S 40)                     */

/* Pin the packing so the stored record never depends on compiler options. */
#pragma pack(push, 8)
struct avmeth {                      /* one validation method                */
     CHAR cls[KEYSIZ];               /*   class to switch to ("" = none)     */
     CHAR key[KEYSIZ];               /*   key to give ("" = none)            */
     LONG creds;                     /*   credits to post                    */
     SHORT paid;                     /*   1 = post as PAID, 0 = FREE         */
};

struct avsetdat {                    /* the settings themselves              */
     SHORT layout;                   /*   AVS_LAYOUT when written            */
     CHAR vldkey[KEYSIZ];            /*   VALIDKEY                           */
     CHAR cfgkey[KEYSIZ];            /*   CNFGACES                           */
     SHORT emlmeth;                  /*   EMAILMTD  1-5                      */
     SHORT maxemail;                 /*   MAXEMAIL  1-100                    */
     SHORT maxatmpt;                 /*   MAXATMPT  0-100 (0 = no limit)     */
     SHORT asklogon;                 /*   AVATLOGN  yes/no                   */
     CHAR globchr;                   /*   AVGLOB    global command trigger   */
     CHAR emltype[AVS_TYPSIZ];       /*   EMAILTYP                           */
     CHAR emlpfx[AVS_PFXSIZ];        /*   EMAILPFX                           */
     CHAR bademail[AVS_BADSIZ];      /*   BADEMAIL                           */
     CHAR logfile[AVS_LOGSIZ];       /*   LOGFILE                            */
     struct avmeth meth[HVS_NMETH];  /*   MTDnCLS / KEY / CRED / PAYM        */
};

struct avsetrec {                    /* the record as stored                 */
     CHAR recid[16];                 /*   AVS_RECID (key 0)                  */
     struct avsetdat d;
     CHAR spare[AVS_RECLEN - 16 - sizeof(struct avsetdat)];
};
#pragma pack(pop)
typedef CHAR avs_chk_size[(sizeof(struct avsetrec) == AVS_RECLEN) ? 1 : -1];

extern struct avsetdat avcfg;        /* live settings - read at point of use */

VOID  set_open(VOID);                /* open file, load or seed settings     */
VOID  set_close(VOID);
VOID  set_defaults(VOID);            /* reload from the CNF options and save */

/* ---- Online editor support (used by AVSYSOP.C) ----                       */

#define SETPG_GENERAL 0              /* page 0 = general, 1..5 = methods     */

INT         set_count(INT page);     /* settings on a page                   */
INT         set_index(INT page, INT n);    /* table index of item n (1-based)*/
const CHAR *set_label(INT idx);
const CHAR *set_value(INT idx);      /* current value, formatted             */
const CHAR *set_limit(INT idx);      /* what values are allowed              */
const CHAR *set_title(INT page);     /* page heading                         */
INT         set_change(INT idx, const CHAR *text);  /* see AVSET.C           */

#define SETCHG_OK     0              /* set_change() results                 */
#define SETCHG_BAD    1              /*   value not allowed                  */
#define SETCHG_NOCLS  2              /*   no such class                      */
#define SETCHG_SAME   3              /*   empty input: kept as is            */

#endif /* AVSET_H */
