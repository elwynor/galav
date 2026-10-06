/* Build 26.10.06.1 03:08PM */
/*****************************************************************************
 *   AVLOG.C                                 Auto Validator - Activity log    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1995 High Velocity Software, Inc.              *
 *                                                                           *
 *   Appends one dated line per event to the log file named by the LOGFILE   *
 *   setting (GALAV.LOG by default).  An empty setting turns logging off.    *
 *   The file is opened and closed for each line, so a sysop can read or     *
 *   rotate it at any time.                                                  *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "AVSET.H"

VOID
av_log(                              /* write one line to the activity log   */
const CHAR *fmt,                     /*   printf-style text of the event     */
...)
{
     FILE *fp;
     va_list ap;
     CHAR date[16];
     static GBOOL warned = FALSE;    /* audit-trail the failure only once    */

     if (avcfg.logfile[0] == '\0') {
          return;
     }
     if ((fp = fopen(avcfg.logfile, "a")) == NULL) {
          if (!warned) {
               shocst("GALAV LOG FILE ERROR",
                      "Auto Validator could not open %s", avcfg.logfile);
               warned = TRUE;
          }
          return;
     }
     warned = FALSE;
     /* ncdatel() and nctime() may share one static buffer, so copy one.    */
     stzcpy(date, ncdatel(today()), sizeof(date));
     fprintf(fp, "%s %s ", date, nctime(now()));
     va_start(ap, fmt);
     vfprintf(fp, fmt, ap);
     va_end(ap);
     fputc('\n', fp);
     fclose(fp);
}
