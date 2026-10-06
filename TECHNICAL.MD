# Auto Validator - Technical Notes

How Auto Validator 2.2.0 works inside. For installing and configuring, see
`SRC/GALAV/GALAV.RLN` (the sysop manual) and [README.MD](README.MD).

## Architecture

Auto Validator is a standard Major BBS v10 module written in C. All its code
runs on the BBS's single main thread, one input line at a time. Nothing in
the module waits or blocks: each call handles the line just typed, records
where the user is in `usrptr->substt`, and returns.

| File | Responsibility |
|---|---|
| `GALAV.C` | `init__galav`, module registration, renaming 2.1.0's `HVSAV*` data files, routing each input line to the user or sysop side, account-deletion and shutdown hooks |
| `AVVALID.C` | User dialog (address → email → code), sending the code, applying a validation method |
| `AVMAIL.C` | The validation email: From address, subject and body; `[NAME]` placeholders; the online subject/body editor |
| `AVSYSOP.C` | Sysop menu, online settings editor, validation email menu, user look-up, global commands |
| `AVSET.C` / `AVSET.H` | Live settings record, seeding from `GALAV.MSG`, and the settings table that drives the editor |
| `AVUSER.C` / `AVUSER.H` | `GALAVUSR.DAT` access |
| `AVTVARS.C` | Named text variables |
| `AVLOG.C` | Activity log |
| `GALAV.MST` → `GALAV.MSG`, `GALAV.H` | Message template (readable tokens), and the message file and numbers built from it |

### Module hooks

| Hook | Routine | Purpose |
|---|---|---|
| logon (`lonrou`) | `avlogon` | When enabled, asks unvalidated users "validate now?". Returning TRUE keeps the user's next input coming back here during logon. |
| input (`sttrou`) | `avinput` | Sysop key holders go to the sysop side, everyone else to the user side |
| delete account (`dlarou`) | `avdelete` | Removes the user's validation record |
| shutdown (`finrou`) | `avshutdown` | Closes the files |

The sysop global commands are registered with `globalcmd()`. The handler
claims a line only when its first word is exactly the trigger character
plus one of `V D E L O ?`, typed by a holder of the sysop key.

### User states

`ST_ENTRY` (0) → `ST_EMLCONT` (begin? Y/N) → `ST_EMLADDR` (address) → done.
On a later visit: `ST_ENTRY` → `ST_CODE` → done. Wrong codes are counted in
the user record (`badcodes`); every third one adds a failed attempt, so
leaving and coming back does not reset the count.
`ST_LOGONQ` is the logon question. The sysop side uses `ST_SYSMENU` and
higher. Per-visit scratch data (the settings page and item being edited, and
the email editor's buffers) lives in the engine's per-user volatile area (`vdaptr`, sized with
`dclvda()`). It is cleared each time the user enters, so it never carries
over from a previous user on the same channel.

## Data files

### GALAVUSR.DAT - user records (200 bytes, page size 1024)

This layout is byte-for-byte the one used by Auto Validator 2.0 for NT,
which was built with 2-byte packing. `AVUSER.H` enforces it with
`#pragma pack(2)` and compile-time size and offset checks. Fields marked
*retired* belonged to the phone call-back feature. They are kept only to
preserve the layout.

| Offset | Field | Notes |
|---|---|---|
| 0 | `userid[30]` | Key 0, unique |
| 30 | `area_code[4]`, `prefix[4]`, `number[5]` | Key 1, three segments, duplicates allowed; *retired* |
| 43 | `email[60]` | Key 2, duplicates allowed; stored upper case |
| 103 | `valid_phone[30]` | *retired* |
| 134 | `code` (SHORT) | Pending code, shown as 4 hex digits; `-1` = none |
| 136 | `method` (SHORT) | Method to apply when the code is entered |
| 138 | `validated` (SHORT) | 1 once validated |
| 140 | `dial_one`, `dial_area` | *retired* |
| 144 | `attempts` (SHORT) | Failed validations |
| 146 | `override` (SHORT) | Ignore the attempt and shared-address limits |
| 148 | `badcodes` (SHORT) | Wrong codes not yet counted as a failure (2.2.0; spare before, so out-of-range values are read as 0) |
| 150 | `spare[49]` | |

The 2.0 file used a case-insensitive collating sequence on keys 0 and 2.
Files created by 2.1.0 do not. Email addresses are stored upper case, and
sysop-typed user-ids are first resolved to the exact user-id through the
BBS account file. Both file versions therefore behave the same.

### GALAVSET.DAT - settings (one 512-byte record, key `SETTINGS`, page size 1024)

`struct avsetrec` holds `struct avsetdat` plus spare space for future
settings. A record must fit a 1024-byte Btrieve page with Btrieve's own
overhead; a 1024-byte record fails with status 28. Its `layout` field allows
later versions to convert the record. Layout 2 (2.2.0) added `fromadr` and
`emlsubj` at the end of `struct avsetdat`, in what was spare space; a layout 1
record is given their CNF defaults when it is loaded. The settings now use
about 465 of the 512 bytes.
On first start the record is built from the CNF options (`set_defaults`).
After that, the in-memory copy `avcfg` is the live configuration. It is read
at the point of use and saved on every change.

## Validation codes and email

- Codes come from `rand_s()` and are kept in the 16-bit `code` field (shown
  as `%04X`). `0xFFFF` is reserved for "no code". This keeps codes issued by
  2.0 working after an upgrade.
- The email is sent with GME's `simpsnd()`, which never waits. `GMEAGAIN`
  means GME queued the message for its background sender, and it counts as
  success. The recipient is the address prefix setting (default `IN:`)
  followed by the user's address.
- **From.** With the `FROMADDR` setting blank the message is from `Sysop`,
  and the SMTP gateway writes `sysop@<its host>` (its `cvtFrom()`). With an
  address set, `msg.from` is the prefix plus that address. The gateway strips
  its own prefix from a sender that starts with it and uses the rest
  unchanged as both the `From:` header and `MAIL FROM` (WG33 `SMTPUTIL.C:154`,
  `SMTPSEND.C:725`); it sends its own bounces the same way
  (`SMTPSEND.C:1406`). GME does not check the sender (`GMECORE.C` `gsndmsg`).
- **Subject** is the `EMLSUBJ` setting. **Body** is `GALAVEML.TXT` if it
  exists, otherwise message `YOUVAL`.
- In the subject and `GALAVEML.TXT`, `[NAME]` is turned into a real text
  variable (0x01 `N` width NAME 0x01) only when `findtvar()` knows NAME,
  so other bracketed text is left alone. `YOUVAL`'s variables are turned
  back into `[NAME]` when it is loaded into the editor.
- The text then goes through `xlttxv()` (never `prfmsg()`), ANSI is
  stripped, line ends become `\r`, and the body is cut to GME's `TXTLEN`.

## Online email editor

The sysop menu's Validation email option opens the BBS editor with
`bgnedt()`: the topic is the subject (`TPCSIZ`) and the text is the body
(4000 bytes). Both buffers are in the module's `vdaptr` area, which the
editor does not use (it keeps its own per-user state and uses only
`vdatmp`). The editor puts the user in its own module state. The done
routine `edone()` puts the user back in Auto Validator's state, saves on
exit (`ED_QUITEX` means the sysop quit), shows the email menu, sends the
output, and returns 1 to leave the editor.

## Text variables and `%`

`prfmsg()` expands text variables first and then uses the result as a
`vsprintf` format string. A `%` inside a variable's value would therefore be
taken as a format code. `AVTVARS.C` returns every value with `%` doubled.
`tv_raw(TRUE)` turns the doubling off while the email body is built with
`xlttxv()`. Message text must never contain a single `%`; write `%%`.

Registered variables: `GALAV_USERID GALAV_EMAIL GALAV_EMAILTYP GALAV_CODE
GALAV_CREDITS GALAV_COUNT GALAV_ATTEMPTS GALAV_VALID GALAV_OVERRIDE
GALAV_METHOD GALAV_ITEM GALAV_LABEL GALAV_VALUE GALAV_PAGE GALAV_LIMIT
GALAV_GLOB GALAV_FROM GALAV_SUBJECT GALAV_BODYSRC GALAV_VERSION`. Up to 2.1.0
they were named `HVS_*`.
The engine's own `SYSTEM_NAME` is used for the board's name.

## Applying a validation method (`av_validate`)

1. Post credits with `addcrd()` (free or paid).
2. Switch class with `swtcls()`. Before that, `fndcls()` must confirm the
   class exists, because `swtcls()` deletes the account when the class is
   missing. The account passed to `swtcls()` is the one the engine is
   actually using: the caller's own `usaptr`, the live copy of another
   online user (`onbbs()` + `uacoff()`), or the stored account.
3. Give the key with `givkey()`.
4. Mark the record validated.

## Message file numbering

`GALAV.H` must match `GALAV.MSG` exactly. Every named entry in `GALAV.MSG`,
including each `LEVELn` marker, takes the next number counting from 0. The
`LEVELn` markers get no `#define`. This matches the numbering of the
Galacticomm WGSMSX tool, and was checked against all 196 numbers of the
original 2.0 header.
