/* Build 26.10.01.1 03:48PM */
/*****************************************************************************
 *   AVUSER.H                           Auto Validator - User records file    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1995 High Velocity Software, Inc.              *
 *                                                                           *
 *   HVSAVUSR.DAT holds one record per user who has used the Auto            *
 *   Validator.  The record layout is byte-for-byte the one Auto Validator   *
 *   2.0 used on Worldgroup for NT, so a board upgrading from 2.0 keeps its  *
 *   existing file.  Fields that belonged to the retired phone-callback      *
 *   feature stay in place (unused) to preserve that layout.                 *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#ifndef AVUSER_H
#define AVUSER_H

#include <stddef.h>
#include "AVDEFS.H"

#define AVU_FILE    "HVSAVUSR.DAT"
#define AVU_EMLSIZ  60               /* email address field, incl. the NUL   */
#define AVU_RECLEN  200              /* fixed record length (2.0 compatible) */

/* Keys, as in the 2.0 file:  0 = user-id,  1 = old phone number (area code, */
/* prefix, number - unused now),  2 = email address.                          */
#define AVU_KEY_UID   0
#define AVU_KEY_EMAIL 2

/* 2.0 was built with 2-byte structure packing, which puts the SHORTs at      */
/* offset 134 and pads the record to exactly 200 bytes.  Match it exactly.    */
#pragma pack(push, 2)
struct avuser {
     CHAR userid[UIDSIZ];            /*   0  user-id (key 0)                 */
     CHAR area_code[4];              /*  30  retired: phone area code        */
     CHAR prefix[4];                 /*  34  retired: phone prefix           */
     CHAR number[5];                 /*  38  retired: phone number           */
     CHAR email[AVU_EMLSIZ];         /*  43  email address, upper case (key 2)*/
     CHAR valid_phone[30];           /* 103  retired: number validated from  */
     SHORT code;                     /* 134  pending code, HVS_NOCODE if none*/
     SHORT method;                   /* 136  method to apply (1-5)           */
     SHORT validated;                /* 138  1 once validated                */
     SHORT dial_one;                 /* 140  retired                         */
     SHORT dial_area;                /* 142  retired                         */
     SHORT attempts;                 /* 144  failed validations so far       */
     SHORT override;                 /* 146  1 = ignore attempt/email limits */
     CHAR spare[51];                 /* 148  unused                          */
};
#pragma pack(pop)

/* Compile-time layout checks: if any of these fail, the record no longer     */
/* matches existing HVSAVUSR.DAT files.                                       */
typedef CHAR avu_chk_size[(sizeof(struct avuser) == AVU_RECLEN) ? 1 : -1];
typedef CHAR avu_chk_email[(offsetof(struct avuser, email) == 43) ? 1 : -1];
typedef CHAR avu_chk_code[(offsetof(struct avuser, code) == 134) ? 1 : -1];
typedef CHAR avu_chk_ovr[(offsetof(struct avuser, override) == 146) ? 1 : -1];

VOID  avu_open(VOID);                /* open (creating if needed)            */
VOID  avu_close(VOID);
GBOOL avu_get(const CHAR *userid, struct avuser *rec);    /* read by user-id */
VOID  avu_new(const CHAR *userid, struct avuser *rec);    /* add a new record*/
VOID  avu_put(struct avuser *rec);   /* update (record must already exist)   */
VOID  avu_delete(const CHAR *userid);
INT   avu_emailcount(const CHAR *email, const CHAR *except);  /* validated   */

#endif /* AVUSER_H */
