# Changelog

All notable changes to Auto Validator are documented here. Versions follow
[Semantic Versioning](https://semver.org/).

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
