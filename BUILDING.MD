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
- **Python 3** for the message-file and packaging tools in `TOOLS/`.

> **Build for x86 (Win32) only. Never x64.** The Major BBS v10 is a 32-bit
> program and cannot load a 64-bit module.

## Setup

Copy `MBBSSDK.props.example` to `MBBSSDK.props` and set `MbbsSdkDir` to your
SDK folder (trailing backslash required). `MBBSSDK.props` is not committed.

## Message file

The module's text and configuration options live in
`SRC/HVSAV/HVSAV.MST`. This is a template in which `<ESC>` stands for the
ANSI escape character and `<TV:NAME>` for a named text variable. Do not edit
`HVSAV.MSG` or `HVSAV.H` by hand. Edit the template and run:

```
python TOOLS/MKMSG.PY
```

This writes `HVSAV.MSG` (CRLF line endings, as the configuration editor
requires) and the message-number header `HVSAV.H`, numbered exactly as the
Galacticomm WGSMSX tool would number them. Rebuild the DLL whenever the
message list changes, and deploy the DLL and `.MSG` together. The BBS
compiles `HVSAV.MCV` from the `.MSG` itself.

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
2. Run:

   ```
   python TOOLS/MKDIST.PY
   ```

   This regenerates the message file and rebuilds Release. It checks that
   the DLL's version matches `HVS_VERSION`, then copies the deployable files
   (`HVSAV.DLL`, `HVSAV.MDF`, `HVSAV.MSG`, `HVSAV.TXT`) into
   `DIST/<version>/` and zips them as `DIST/HVSAV-<version>.zip`.

The script refuses to overwrite an existing `DIST/<version>/` folder. A
version number must identify exactly one build, so bump the version rather
than rebuilding under an existing number.

## Release checklist

- Project builds with no errors or warnings.
- Version numbers updated (`AVDEFS.H`, `HVSAV.rc`, `CHANGELOG.MD`).
- `README.MD` and `HVSAV.TXT` updated.
- `DIST/<version>/` holds only the four deployable files.
- DLL properties show the right product, version, company and copyright.
- Installation tested on a test board.
- No credentials, internal paths or internal notes in published files.
