# 3rd Party Software Acknowledgments

The Limine project depends on several other projects.

(For readers with access to source code, know that these are pulled in by the
`./bootstrap` script, or, in the case of release tarballs, are shipped
alongside the core Limine code in the tarballs themselves, similar to
`./bootstrap` having already been run.)

These additional projects are NOT covered by the License as contained inside
the `COPYING` file as present at the root of the source tree, or, for installed
copies, present at `${DOCDIR}/COPYING` (assuming the file has not been
otherwise removed by the packager). These are instead licensed as described by
each individual project's documentation present in each project's dedicated
subdirectory or license header(s) in the source tree. For readers without access
to the source code, one can read the following for a quick overview of licenses
that Limine is distributed under:

A non-binding, informal summary of all projects Limine depends on, and the
licenses used by said projects, in SPDX format, is as follows:

- [cc-runtime](https://github.com/osdev0/cc-runtime)
(Apache-2.0 WITH LLVM-exception) is used to provide runtime libgcc-like
routines.

- [Freestanding C Headers](https://github.com/osdev0/freestanding-c-hdrs)
(0BSD) provide GCC and Clang compatible freestanding C headers.

- [Limine Boot Protocol](https://github.com/Limine-Bootloader/limine-protocol)
(0BSD) has the C/C++ header and the specification text of the Limine Boot
Protocol.

- [PicoEFI](https://github.com/PicoEFI/PicoEFI) (multiple licenses, see list
below) provides headers and build-time support for UEFI.
  - BSD-2-Clause
  - BSD-2-Clause-Patent
  - BSD-3-Clause
  - LicenseRef-scancode-bsd-no-disclaimer-unmodified
  - MIT

    For more information about the
    LicenseRef-scancode-bsd-no-disclaimer-unmodified license used by parts of
    PicoEFI, see
    <https://scancode-licensedb.aboutcode.org/bsd-no-disclaimer-unmodified.html>
    and the
    [LicenseRef file](LICENSES/LicenseRef-scancode-bsd-no-disclaimer-unmodified.txt),
    in case of viewing this file from inside the source tree, alternatively at
    `${DOCDIR}/LICENSES/LicenseRef-scancode-bsd-no-disclaimer-unmodified.txt`
    in case of installed copies, assuming the file has not been otherwise
    removed by the packager.

- [FreeType](https://freetype.org) (FTL) renders TrueType outlines in the
Eucleia graphical menu. Version 2.14.3 is pinned at
`0a0221a1347e2f1e07c395263540026e9a0aa7c7`. Portions of this software are
copyright (C) 2026 The FreeType Project (<https://freetype.org>).
All rights reserved. The FreeType License is selected from its dual licence;
see `freetype/docs/FTL.TXT` (installed as `LICENSES/FTL.TXT`).
FreeType source and its embedded third-party notices are retained unchanged.
The included hashing code uses an MIT-style licence; its notice and the
HarfBuzz-derived notices are also in `LICENSES/FreeType-third-party.txt`.

- [Flanterm](https://github.com/Mintsuki/Flanterm) (BSD-2-Clause) is used for
text related screen drawing.

- [stb_image (hardened)](https://github.com/Mintsuki/stbi-hardened) (MIT) is
used for wallpaper image loading.

- [libfdt](https://github.com/osdev0/libfdt) (BSD-2-Clause) is used for
manipulating Flat Device Trees.

- [pdgzip](https://github.com/iczelia/pdgzip) (0BSD) is used to provide the
transparent gzip decompression layer for loaded files.

Note that some of these projects, or parts of them, are provided under
dual-licensing, in which case, in the above list, the only license mentioned is
the one chosen by the Limine developers. Refer to each individual project's
documentation for details.

The optional Classical theme includes Cinzel and EB Garamond under the SIL
Open Font License 1.1. Source packages retain their original licences in
`themes/classical/fonts`; binary packages include both font licences under
`theme/fonts`. Font sources are recorded in the theme's `SOURCES.md`.
FontTools and Pillow are host-side theme
build tools and are not linked into the bootloader.

The Classical icon set adapts operating-system outlines from
[font-logos](https://github.com/Lukas-W/font-logos) (Unlicense), pinned at
`d3bf5d299e54595db1b19681a0cc57ab10454857`. The Gentoo outline carries its own
CC-BY-SA-2.5 notice, which also covers the themed Gentoo derivative. Original
source links, licences and attribution are retained in `themes/classical`
(packaged as `theme`); see `SOURCES.md` and `icons/README.md`.
Product names and logos belong to their respective owners.

The upstream test wallpaper (`test/bg.jpg`) is a photograph by Levent Simsek,
credited by upstream to
<https://www.pexels.com/photo/brown-tabby-cat-in-close-up-photography-3617160/>.
It is retained only as an upstream test fixture in source packages.
