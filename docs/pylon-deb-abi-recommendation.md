# Recommendation: encode C++ SDK 12 ABI in the pylon Debian package

**To:** pylon Linux / Debian package maintainers  
**From:** gst-plugin-pylon  
**Subject:** Encode C++ SDK 12 ABI in the `pylon` deb (`shlibs` + `Provides`), keep the date `Version`

## Problem

Downstream packages (including gst-plugin-pylon) need **binary compatibility with pylon C++ SDK 12.x** (`libpylonbase.so.12`), not with a calendar release.

The current Linux Debian package uses a **date `Version`**, which is fine for the product, but it does not expose ABI at all.

Inspecting `pylon-26.08.1_linux-x86_64_debs.tar.gz` → `pylon_26.08.1-deb0_amd64.deb`:

| Layer | What we found |
| --- | --- |
| dpkg | `Package: pylon`, `Version: 26.08.1-deb0` |
| C++ SDK | CMake `12.3.0.1362`, `PYLON_VERSION_MAJOR` 12, suffix `_v12` |
| SONAME | `libpylonbase.so.12` → `libpylonbase.so.12.3.0` (same for `libpylonutility`) |
| Control extras | **No** `Provides`, **no** `shlibs`, **no** `symbols` |

Suite **26.06** already shipped SDK **12.2.1** with SONAME `.so.12`. Suite **26.04** was still C++ SDK **11.5**. Release notes for 26.06 and 26.08 both list C++ SDK binary compatibility as **12.0.0** (compatible with other 12.x).

We therefore cannot write a correct `Depends` on ABI using `Version:`:

- `pylon (>= 12), pylon (<< 13)` **rejects** `26.08.1-deb0` (dpkg compares 26.08 to 13).
- `pylon (<< 27)` **rejects a future 27.xx** that may still be `.so.12`.

Today we can only use a date floor (`pylon (>= 26.06)` = first suite with SDK 12) and enforce 12.x at **build** time via CMake. That is a guess, not a dpkg ABI contract.

Debian Policy (chapter 8) puts ABI in the **package name / Provides / shlibs**, not in a marketing `Version`.

## Recommendation (keep the date schema)

Do **not** put `12.3.0` into `Version:`. Keep `26.08.1-deb0`. Add ABI metadata beside it.

### 1. `shlibs` (needed so `dh_shlibdeps` works)

Ship a shlibs file for the C++ runtime SONAMEs, for example:

```
libpylonbase 12 pylon (>= 26.06)
libpylonutility 12 pylon (>= 26.06)
```

`26.06` is the first suite that provided SONAME 12. When SONAME becomes `.so.13`, change the `12` and the dependency on that line.

Without this, consumers must `--ignore-missing-info` and hard-code `Depends`.

### 2. Virtual ABI package (what downstream should Depend on)

```
Package: pylon
Version: 26.08.1-deb0
Provides: pylon-abi-12
```

Downstream then uses:

```
Depends: pylon-abi-12
```

Any later date (`26.12`, `27.01`, …) stays valid **as long as SONAME stays 12**. On an ABI break:

```
Provides: pylon-abi-13
```

and drop `pylon-abi-12` so old binaries do not install against a 13-only runtime.

Do not use `Provides: pylon-abi-12 (= 12.3.0)` unless you intend to constrain the *minor*. The contract we need is SONAME 12.

### 3. Optional later: split runtime vs SDK

Policy-ideal would be `libpylonbase12` / `libpylonutility12` + `libpylon-dev` + a `pylon` metapackage. That is a larger installer change (`/opt/pylon`). **(1) + (2) are enough** without moving files.

A C++ `symbols` file is not recommended here; `shlibs` is the right tool.

## What gst-plugin-pylon will do as a consumer

Until `Provides` / `shlibs` exist: `Depends: pylon (>= 26.06)` and Meson `C++ SDK >= 12.2, < 13`.

Once `pylon-abi-12` and shlibs ship: switch to `Depends: pylon-abi-12` (or `${shlibs:Depends}`) and drop suite-date ceilings.

## Ask

Please add **`Provides: pylon-abi-12`** and the **`libpylonbase 12` / `libpylonutility 12` shlibs** to the Linux Debian packages, and keep using the date `Version`. Treat SONAME 12 as the compatibility ID; bump Provides/shlibs only when SONAME changes.
