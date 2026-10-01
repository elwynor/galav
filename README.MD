# Auto Validator for The Major BBS v10

**Auto Validator** (`HVSAV`) validates new users on The Major BBS v10 by email.
A new user gives their email address, receives a validation code, and enters
it on their next visit. They are then upgraded automatically: switched to a
new class, given a key, and/or posted credits, as the sysop configures.

Auto Validator was originally written by High Velocity Software for Worldgroup.
Version 2.1.0 is a rewrite for The Major BBS v10 by Elwynor Technologies.

## Features

- **Email validation.** Codes go out through your BBS's Internet mail
  gateway. Users get three tries per visit.
- **Five validation methods.** Each method sets a class change, a key, and a
  credit grant (free or paid). The sysop picks which method email validations
  use, and can apply any method by hand.
- **Online settings.** Every setting can be changed from the in-module sysop
  menu while the board is running. Changes apply immediately and are recorded
  in the Audit Trail.
- **Abuse limits.** You can cap how many validated users share one email
  address, limit failed attempts per user, refuse a domain, and refuse
  addresses listed in an optional `HVSAVBAD.TXT` file.
- **Sysop global commands.** These work from anywhere on the board:
  validate (`@V`), devalidate (`@D`), resend the code (`@E`), look up a user
  (`@L`), and toggle the override (`@O`).
- **Logon offer.** Optionally, unvalidated users are asked at logon whether
  they want to validate now.
- **Activity log.** Every validation, failure and sysop action is written to a
  dated text log.
- **Fully editable text.** All user-facing text, including the email itself,
  is in `HVSAV.MSG`. Values such as the user's address use named text
  variables, so sysops can reword messages freely.
- **Upgrade friendly.** User records from Auto Validator 2.0 are read as they
  are.

## Installation

1. Copy `HVSAV.DLL`, `HVSAV.MDF`, `HVSAV.MSG` and `HVSAV.TXT` from the
   release folder into your BBS directory.
2. Add Auto Validator to your menu with the Menu Tree editor.
3. Make sure your Internet mail gateway can send mail out.
4. Start the BBS. Auto Validator creates `HVSAVUSR.DAT` and `HVSAVSET.DAT`
   on first start.

**Upgrading from 2.0:** keep your `HVSAVUSR.DAT`. `HVSAVCFG.DAT` and
`HVSAVNVR.DAT` are no longer used. No activation code is needed.

## Configuration

Enter Auto Validator with the sysop key (`SYSOP` by default) and choose
**Settings**. The general settings cover the request and sysop keys, the
method used for email validations, the shared-address and failed-attempt
limits, the logon offer, the global command trigger, email wording and
routing, and the log file. Each of methods 1-5 has a class, a key, a credit
amount and a credit type.

The level 3 and 4 options in the configuration editor are **install
defaults** only. They are copied into the settings file the first time the
module starts. See `HVSAV.TXT` for every setting.

## Usage

- **Users:** select Auto Validator from the menu, enter your email address,
  and come back with the code from the email.
- **Sysops:** use the sysop menu to change settings and look up users, and
  the global commands (`@?` for the list) to validate or reset users from
  anywhere.

## Version Information

Current version: **2.1.0**. See [CHANGELOG.MD](CHANGELOG.MD) for changes and
[BUILDING.MD](BUILDING.MD) to build from source.

## License

Auto Validator is licensed under the
[GNU Affero General Public License v3.0](LICENSE). See
[CONTRIBUTING.MD](CONTRIBUTING.MD) to contribute.

Copyright © Elwynor Technologies. Originally © High Velocity Software, Inc.

## Support

Please report problems and suggestions through the GitHub issue tracker for
this repository.

---

Auto Validator converted for The Major BBS v10 by Mark Laudenbach - https://github.com/laudenbachm
