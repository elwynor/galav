/* Build 26.10.01.1 03:48PM */
/*****************************************************************************
 *   AVTVARS.C                            Auto Validator - Text variables     *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Every changing value in HVSAV.MSG (a user-id, an email address, a       *
 *   code) is a named text variable - the byte sequence                      *
 *   0x01 <justify> <width+32> NAME 0x01 - rather than a printf-style %s.    *
 *   That lets sysops reword, reorder or drop values when they edit the      *
 *   text.  The engine replaces each variable with the string returned by    *
 *   the routine registered for its name.  We fill those strings with        *
 *   tv_set() just before printing a message.                                *
 *                                                                           *
 *   One trap: prfmsg() expands variables FIRST and then runs the result     *
 *   through vsprintf() as a format string.  A '%' inside a value (an email  *
 *   address, or text a sysop typed) would be read as a format code and      *
 *   could crash the board.  So values are returned with every '%' doubled.  *
 *   For text that does not go through prfmsg() - the validation email,      *
 *   built with xlttxv() - call tv_raw(TRUE) to turn the doubling off.       *
 *                                                                           *
 *   Names are global to the whole BBS, so all of ours start with HVS_.     *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "AVDEFS.H"

#define TVBSIZ 80                    /* room for any value we show           */

static CHAR tvval[TV_COUNT_OF][TVBSIZ];        /* values as set              */
static CHAR tvout[2 * TVBSIZ];                 /* value as returned          */
static GBOOL tvraw = FALSE;                    /* TRUE: no %% doubling       */

static CHAR *
tvget(INT which)                     /* return a value, escaped for prfmsg() */
{
     const CHAR *s = tvval[which];
     CHAR *d = tvout;

     if (tvraw) {
          return tvval[which];
     }
     while (*s != '\0') {
          if (*s == '%') {
               *d++ = '%';
          }
          *d++ = *s++;
     }
     *d = '\0';
     return tvout;
}

/* The engine needs a separate routine for each registered name.            */
static CHAR *tvUSERID(VOID)   { return tvget(TV_USERID);   }
static CHAR *tvEMAIL(VOID)    { return tvget(TV_EMAIL);    }
static CHAR *tvEMAILTYP(VOID) { return tvget(TV_EMAILTYP); }
static CHAR *tvCODE(VOID)     { return tvget(TV_CODE);     }
static CHAR *tvCREDITS(VOID)  { return tvget(TV_CREDITS);  }
static CHAR *tvCOUNT(VOID)    { return tvget(TV_COUNT);    }
static CHAR *tvATTEMPTS(VOID) { return tvget(TV_ATTEMPTS); }
static CHAR *tvVALID(VOID)    { return tvget(TV_VALID);    }
static CHAR *tvOVERRIDE(VOID) { return tvget(TV_OVERRIDE); }
static CHAR *tvMETHOD(VOID)   { return tvget(TV_METHOD);   }
static CHAR *tvITEM(VOID)     { return tvget(TV_ITEM);     }
static CHAR *tvLABEL(VOID)    { return tvget(TV_LABEL);    }
static CHAR *tvVALUE(VOID)    { return tvget(TV_VALUE);    }
static CHAR *tvPAGE(VOID)     { return tvget(TV_PAGE);     }
static CHAR *tvLIMIT(VOID)    { return tvget(TV_LIMIT);    }
static CHAR *tvGLOB(VOID)     { return tvget(TV_GLOB);     }
static CHAR *tvVERSION(VOID)  { return HVS_VERSION;        }

VOID
tv_init(VOID)                        /* register all of our text variables   */
{
     register_textvar("HVS_USERID",   tvUSERID);
     register_textvar("HVS_EMAIL",    tvEMAIL);
     register_textvar("HVS_EMAILTYP", tvEMAILTYP);
     register_textvar("HVS_CODE",     tvCODE);
     register_textvar("HVS_CREDITS",  tvCREDITS);
     register_textvar("HVS_COUNT",    tvCOUNT);
     register_textvar("HVS_ATTEMPTS", tvATTEMPTS);
     register_textvar("HVS_VALID",    tvVALID);
     register_textvar("HVS_OVERRIDE", tvOVERRIDE);
     register_textvar("HVS_METHOD",   tvMETHOD);
     register_textvar("HVS_ITEM",     tvITEM);
     register_textvar("HVS_LABEL",    tvLABEL);
     register_textvar("HVS_VALUE",    tvVALUE);
     register_textvar("HVS_PAGE",     tvPAGE);
     register_textvar("HVS_LIMIT",    tvLIMIT);
     register_textvar("HVS_GLOB",     tvGLOB);
     register_textvar("HVS_VERSION",  tvVERSION);
}

VOID
tv_set(INT which, const CHAR *value)           /* set a text value           */
{
     stzcpy(tvval[which], value, TVBSIZ);
}

VOID
tv_setn(INT which, LONG value)                 /* set a numeric value        */
{
     sprintf(tvval[which], "%ld", value);
}

VOID
tv_raw(GBOOL raw)                    /* TRUE while building non-prfmsg text  */
{
     tvraw = raw;
}
