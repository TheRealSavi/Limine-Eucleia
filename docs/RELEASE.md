# Release preparation

Run from a bootstrapped source tree with a configured out-of-tree build.
The regular build toolchain is described in `INSTALL.md`. Theme construction
adds Python 3.9+, Pillow and FontTools. Source packaging uses Python's standard
library and does not download files.

```sh
make -C build dist
make -C build dist-binary
python3 tests/packaging_test.py build/limine-eucleia-*.tar.gz
```

`dist` creates `limine-eucleia-VERSION.tar.gz` with generated `configure`,
pinned dependency sources, implementation, documentation, tests, examples and
curated theme sources. It reads the current working tree using
`packaging/source-roots.txt`; no commit is necessary to include new files.
Review the working tree before making a release. Unknown top-level folders,
local archives, concept work, build outputs and Git metadata are excluded.

`dist-binary` builds the configured firmware targets and theme, then packages
those binaries, the portable host-tool source, UI configuration, icons and
required notices. It does not promise unconfigured architectures. Its archive
is named `limine-eucleia-VERSION-binary.tar.gz`. Each archive has a SHA-256
sidecar and an internal `RELEASE-MANIFEST.json` with per-file hashes.

Packaging normalises timestamps, ownership, modes and file order. Repeating
packaging with identical inputs and timestamp produces identical archive bytes.
This does not assert cross-toolchain reproducibility of compiled firmware.
Archives can be rebuilt offline from the source package using `./configure`;
there is no need to run bootstrap again. Packaging never copies `.git` or
modifies the developer's working tree.

## Verify a candidate

1. Run host checks and applicable VM suites in `tests/ui/README.md`.
2. Create the source archive and inspect its manifest.
3. Extract it into an empty directory, configure the desired targets and build.
4. Build its theme and binary archive; compile the packaged host tool.
5. Boot a loader and theme from that clean build in a private QEMU guest.
6. Stage `make install DESTDIR=...` into a temporary directory if checking
   installation; never use a live system prefix for release validation.

The upstream base version is retained until the maintainer chooses a fork
release tag. An archive produced during development is a development snapshot,
not an announced stable release. Use a distinct Eucleia release tag when
publishing; do not present a fork package as an upstream release.

The manual release workflow builds and uploads candidate archives to the CI
run. Publication, tags and signing credentials are controlled by the maintainer.
Local packaging does not push, publish or sign a release.
