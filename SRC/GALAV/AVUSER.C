/* Build 26.10.06.1 03:08PM */
/*****************************************************************************
 *   AVUSER.C                           Auto Validator - User records file    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1995 High Velocity Software, Inc.              *
 *                                                                           *
 *   Reads and writes GALAVUSR.DAT.  Every routine selects our file with     *
 *   dfaSetBlk() and restores the previous selection with dfaRstBlk(), so    *
 *   callers never have to think about which Btrieve file is current.        *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include "gcomm.h"
#include "majorbbs.h"
#include "AVUSER.H"

static DFAFILE *avudat;              /* GALAVUSR.DAT                         */

VOID
avu_open(VOID)                       /* open the user file, create if absent */
{
     if (!isfile(AVU_FILE)) {
          /* Recreate the 2.0 file definition (HVSAVUSR.BCR):                */
          /*   key 0  user-id, unique                                        */
          /*   key 1  area code + prefix + number, duplicates (unused now)  */
          /*   key 2  email address, duplicates                             */
          /* Email addresses are stored upper case, so key 2 needs no       */
          /* case-insensitive collating sequence.                           */
          struct dfaSegSpec uid, phone[3], email;
          struct dfaKeySpec keys[3];

          setmem(&uid, sizeof(uid), 0);
          uid.position = offsetof(struct avuser, userid);
          uid.length = UIDSIZ;
          uid.type = DFAST_ZSTRING;

          setmem(phone, sizeof(phone), 0);
          phone[0].position = offsetof(struct avuser, area_code);
          phone[0].length = 4;
          phone[1].position = offsetof(struct avuser, prefix);
          phone[1].length = 4;
          phone[2].position = offsetof(struct avuser, number);
          phone[2].length = 5;
          phone[0].type = phone[1].type = phone[2].type = DFAST_ZSTRING;

          setmem(&email, sizeof(email), 0);
          email.position = offsetof(struct avuser, email);
          email.length = AVU_EMLSIZ;
          email.type = DFAST_ZSTRING;

          setmem(keys, sizeof(keys), 0);
          keys[0].flags = 0;
          keys[0].nSegments = 1;
          keys[0].segs = &uid;
          keys[1].flags = DFAKF_DUPLICATE | DFAKF_MODIFYABLE;
          keys[1].nSegments = 3;
          keys[1].segs = phone;
          keys[2].flags = DFAKF_DUPLICATE | DFAKF_MODIFYABLE;
          keys[2].nSegments = 1;
          keys[2].segs = &email;

          dfaCreateSpec(AVU_FILE, FALSE, sizeof(struct avuser), 1024,
                        0, 0, 3, keys, NULL);
     }
     avudat = dfaOpen(AVU_FILE, sizeof(struct avuser), NULL);
}

VOID
avu_close(VOID)
{
     dfaClose(avudat);
}

GBOOL
avu_get(                             /* read a user's record                 */
const CHAR *userid,                  /*   user-id to read                    */
struct avuser *rec)                  /*   where to put it                    */
{
     CHAR key[UIDSIZ];
     GBOOL found;

     stzcpy(key, userid, UIDSIZ);
     dfaSetBlk(avudat);
     found = dfaAcqEQ(rec, key, AVU_KEY_UID);
     dfaRstBlk();
     return found;
}

VOID
avu_new(                             /* create a fresh record for a user     */
const CHAR *userid,
struct avuser *rec)                  /*   receives the new record            */
{
     setmem(rec, sizeof(*rec), 0);
     stzcpy(rec->userid, userid, UIDSIZ);
     rec->code = GALAV_NOCODE;
     rec->method = 1;
     dfaSetBlk(avudat);
     dfaInsert(rec);
     dfaRstBlk();
}

VOID
avu_put(                             /* save changes to an existing record   */
struct avuser *rec)
{
     struct avuser cur;

     /* Re-acquire first: the engine's update writes over the record last   */
     /* read from the file, which may not be this one.                       */
     dfaSetBlk(avudat);
     if (dfaAcqEQ(&cur, rec->userid, AVU_KEY_UID)) {
          dfaUpdate(rec);
     }
     dfaRstBlk();
}

VOID
avu_delete(                          /* remove a user (account deleted)      */
const CHAR *userid)
{
     CHAR key[UIDSIZ];

     stzcpy(key, userid, UIDSIZ);
     dfaSetBlk(avudat);
     if (dfaAcqEQ(NULL, key, AVU_KEY_UID)) {
          dfaDelete();
     }
     dfaRstBlk();
}

INT                                  /*   number of validated users found    */
avu_emailcount(                      /* count validated users at an address  */
const CHAR *email,                   /*   address (upper case)               */
const CHAR *except)                  /*   user-id to leave out, or NULL      */
{
     struct avuser rec;
     CHAR key[AVU_EMLSIZ];
     INT count = 0;
     GBOOL more;

     if (email[0] == '\0') {
          return 0;
     }
     stzcpy(key, email, AVU_EMLSIZ);
     dfaSetBlk(avudat);
     /* dfaAcqNXi() stops by itself when the next record's key differs, so  */
     /* this visits exactly the records with this address.                   */
     for (more = dfaAcqEQ(&rec, key, AVU_KEY_EMAIL) ; more ;
          more = dfaAcqNXi(&rec)) {
          if (rec.validated && (except == NULL || !sameas(rec.userid, except))) {
               count++;
          }
     }
     dfaRstBlk();
     return count;
}
