# Auto Validator - Technical Notes

How Auto Validator 2.1.0 works inside. For installing and configuring, see
`SRC/HVSAV/HVSAV.TXT` (the sysop manual) and [README.MD](README.MD).

## Architecture

Auto Validator is a standard Major BBS v10 module written in C. All its code
runs on the BBS's single main thread, one input line at a time. Nothing in
the module waits or blocks: each call handles the line just typed, records
where the user is in `usrptr->substt`, and returns.

| File | Responsibility |
|---|---|
| `HVSAV.C` | `init__hvsav`, module registration, routing each input line to the user or sysop side, account-deletion and shutdown hooks |
| `AVVALID.C` | User dialog (address → email → code), sending the code, applying a validation method |
| `AVSYSOP.C` | Sysop menu, online settings editor, user look-up, global commands |
| `AVSET.C` / `AVSET.H` | Live settings record, seeding from `HVSAV.MSG`, and the settings table that drives the editor |
| `AVUSER.C` / `AVUSER.H` | `HVSAVUSR.DAT` access |
| `AVTVARS.C` | Named text variables |
| `AVLOG.C` | Activity log |
| `HVSAV.MST` → `HVSAV.MSG`, `HVSAV.H` | Message template (readable tokens), and the message file and numbers built from it |

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
On a later visit: `ST_ENTRY` → `ST_CODE` (up to 3 tries) → done.
`ST_LOGONQ` is the logon question. The sysop side uses `ST_SYSMENU` and
higher. Per-visit scratch data (code tries, the settings page and item being
edited) lives in the engine's per-user volatile area (`vdaptr`, sized with
`dclvda()`). It is cleared each time the user enters, so it never carries
over from a previous user on the same channel.

## Data files

### HVSAVUSR.DAT - user records (200 bytes, page size 1024)

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
| 148 | `spare[51]` | |

The 2.0 file used a case-insensitive collating sequence on keys 0 and 2.
Files created by 2.1.0 do not. Email addresses are stored upper case, and
sysop-typed user-ids are first resolved to the exact user-id through the
BBS account file. Both file versions therefore behave the same.

### HVSAVSET.DAT - settings (one 512-byte record, key `SETTINGS`, page size 1024)

`struct avsetrec` holds `struct avsetdat` plus spare space for future
settings. A record must fit a 1024-byte Btrieve page with Btrieve's own
overhead; a 1024-byte record fails with status 28. Its `layout` field allows later versions to convert the record.
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
  followed by the user's address. The subject is message `EMLSUBJ` and the
  body is `YOUVAL`.
- The body never goes through `prfmsg()`. Its text variables are expanded
  with `xlttxv()`, ANSI is stripped, and line ends are converted to `\r`.

## Text variables and `%`

`prfmsg()` expands text variables first and then uses the result as a
`vsprintf` format string. A `%` inside a variable's value would therefore be
taken as a format code. `AVTVARS.C` returns every value with `%` doubled.
`tv_raw(TRUE)` turns the doubling off while the email body is built with
`xlttxv()`. Message text must never contain a single `%`; write `%%`.

Registered variables: `HVS_USERID HVS_EMAIL HVS_EMAILTYP HVS_CODE
HVS_CREDITS HVS_COUNT HVS_ATTEMPTS HVS_VALID HVS_OVERRIDE HVS_METHOD
HVS_ITEM HVS_LABEL HVS_VALUE HVS_PAGE HVS_LIMIT HVS_GLOB HVS_VERSION`.
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

`HVSAV.H` must match `HVSAV.MSG` exactly. Every named entry in `HVSAV.MSG`,
including each `LEVELn` marker, takes the next number counting from 0. The
`LEVELn` markers get no `#define`. This matches the numbering of the
Galacticomm WGSMSX tool, and was checked against all 196 numbers of the
original 2.0 header.
