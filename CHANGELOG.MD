# Changelog

All notable changes to Auto Validator are documented here. Versions follow
[Semantic Versioning](https://semver.org/).

## [2.2.0] - 2026-10-06

### Added
- The From address of the validation email is now a setting, for example
  `validate@example.com` or `validate@bbs.example.com`. Left blank, the
  email is sent as the Sysop, as before.
- The subject and body of the validation email can be edited online. The
  sysop menu's new Validation email option opens the BBS editor on them;
  changes apply to the next email sent. The subject is also a setting, and
  the edited body is kept in `GALAVEML.TXT`. A restore option returns both
  to the defaults.
- `[NAME]` in the subject or body stands for the text variable NAME, such as
  `[GALAV_CODE]` or `[SYSTEM_NAME]`.

### Changed
- The module files are renamed from `HVSAV` to `GALAV`: `GALAV.DLL`,
  `GALAV.MDF`, `GALAV.MSG`, and the data files `GALAVUSR.DAT`,
  `GALAVSET.DAT` and `GALAVBAD.TXT`. Existing `HVSAV` data files are renamed
  automatically on first start. Delete the old `HVSAV` module files when
  upgrading.
- The sysop manual is now `GALAV.RLN` (was `HVSAV.TXT`). The module
  description's online user manual line points to it.
- The configuration editor's level 3, 4 and 6 sections are titled "Auto
  Validator" instead of showing the file name.
- Text variables are renamed from `HVS_*` to `GALAV_*`.
- The email subject moved from a text message (`EMLSUBJ`, level 6) to an
  option (level 4), so it can be set like other settings.
- Every three wrong codes now count as one failed validation even when they
  are spread over several visits.
- The activity log file, when changed online, must be a plain file name in
  the BBS directory.

### Security
- A user could guess validation codes without limit: the wrong-code count
  was kept only for the current visit, so leaving after two wrong codes (or
  hanging up) and coming back reset it, and no failed attempt was ever
  recorded. The count is now kept in the user record.
- A holder of the sysop key could point the activity log at any file on the
  server and append to it. Online changes now accept only a file name in the
  BBS directory.
- The From address read from the settings file is checked again before use,
  so a damaged file cannot put line breaks into the mail header.

## [2.1.0] - 2026-10-01

First release for The Major BBS v10. This is a rewrite on the native Major
BBS v10 module interface. The previous version was 2.01c, for Worldgroup.

### Added
- Online settings editor in the sysop menu. Every setting can be changed
  while the board runs. Changes take effect immediately and are recorded in
  the Audit Trail and the activity log. Settings are stored in a new file,
  `HVSAVSET.DAT`. The configuration-editor options are now install defaults.
- "Reset all settings to the HVSAV.MSG defaults" option.
- Sysop user look-up in the sysop menu, as well as the `@L` command.
- Sysop menu option to list the global commands.
- Dated activity log entries. Settings changes are logged too.
- Windows version information in `HVSAV.DLL`.
- `HVSAV.TXT` sysop manual, referenced from the module description.

### Changed
- Validation is by email only.
- No activation code or demo mode. The activation option has been removed
  from the configuration.
- All dynamic values in messages now use named text variables (`HVS_EMAIL`,
  `HVS_CODE`, ...), so sysops can freely reword the text. The validation
  email's subject line can now be edited.
- `@D` (devalidate) also clears a pending code, so the user starts fresh.
- When the code email cannot be sent, the user may try again on their next
  visit instead of being asked for a code that never arrived.
- The failed-attempts limit now allows exactly the configured number of
  failures. 2.0 allowed one more.
- Email addresses are checked more strictly (one `@`, a dotted domain, no
  spaces or special characters).

### Removed
- Phone call-back validation, the area-code/prefix dialing table, the "never
  call" number list, the dial monitor, secure mode, postal-mail validation
  and mailing labels, and the related global commands (`@M`, `@N`, `@P`,
  `@S`, `@U`) and options.

### Fixed
- An unknown class in a method setting could cause the user's account to be
  deleted at validation time. Such a class is now refused in the settings
  editor and skipped (with an Audit Trail warning) if it disappears later.
- Validation codes were predictable. They now come from a secure random
  source.
- The refused-address file `HVSAVBAD.TXT` was never closed after being read.
- A `%` in an email address or setting could corrupt or crash message
  output.
