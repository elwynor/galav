# Building Auto Validator

Auto Validator is a Win32 DLL module for The Major BBS v10. It is built with
Microsoft Visual Studio against the MBBS v10 Module SDK.

## Prerequisites

- **Microsoft Visual Studio 2022** (or Build Tools 2022) with the C++
  desktop workload. Platform toolset **v143**.
- **Windows 10 SDK** (`WindowsTargetPlatformVersion` 10.0).
- **The Major BBS v10 Module SDK**
  (https://github.com/TheMajorBBS/MBBS-v10-module-SDK). It provides the
  headers (`inc/`) and import libraries (`lib/importlibs`, `lib/v10`).

> **Build for x86 (Win32) only. Never x64.** The Major BBS v10 is a 32-bit
> program and cannot load a 64-bit module.

## Setup

Copy `MBBSSDK.props.example` to `MBBSSDK.props` and set `MbbsSdkDir` to your
SDK folder (trailing backslash required). `MBBSSDK.props` is not committed.

## Message file

The module's text and configuration options are in `SRC/HVSAV/HVSAV.MSG`,
and `SRC/HVSAV/HVSAV.H` holds the matching message numbers. `HVSAV.MSG`
contains raw ESC and 0x01 bytes (ANSI color and named text variables) and
must keep CRLF line endings, so edit it with an editor that preserves
both. `HVSAV.MST` is the same text with those bytes written as readable
`<ESC>` and `<TV:NAME>` tokens.

Message numbers are assigned in file order, starting from 0, and every
named entry counts, including each `LEVELn` marker. If you add, remove or
reorder an entry, update `HVSAV.H` to match and rebuild the DLL. Deploy the
DLL and `.MSG` together. The BBS compiles `HVSAV.MCV` from the `.MSG`
itself.

## Build

In Visual Studio, open `HVSAV.sln`, choose **Release** / **Win32**, and
build. Or from a command prompt:

```
MSBuild.exe HVSAV.sln -p:Configuration=Release -p:Platform=Win32
```

The DLL is written to `BUILD/Release/HVSAV.dll`.

## Packaging a release

1. Set the new version in `SRC/HVSAV/AVDEFS.H` (`HVS_VER_*`, `HVS_VERSION`)
   and `SRC/HVSAV/HVSAV.rc`, and update `CHANGELOG.MD`.
2. Rebuild the Release configuration, and check that the DLL's version
   matches `HVS_VERSION`.
3. Copy only the deployable files into `DIST/<version>/`: `HVSAV.DLL`,
   `HVSAV.MDF`, `HVSAV.MSG` and `HVSAV.TXT`. Zip them as
   `HVSAV-<version>.zip` and attach it to the GitHub release.

Never rebuild under a version number that has already been released.
Bump the version instead.

## Release checklist

- Project builds with no errors or warnings.
- Version numbers updated (`AVDEFS.H`, `HVSAV.rc`, `CHANGELOG.MD`).
- `README.MD` and `HVSAV.TXT` updated.
- `DIST/<version>/` holds only the four deployable files.
- DLL properties show the right product, version, company and copyright.
- Installation tested on a test board.
- No credentials, internal paths or internal notes in published files.
