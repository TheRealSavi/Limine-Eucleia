# Build and Install Limine-Eucleia

For theme deployment, see [the deployment guide](docs/DEPLOYMENT.md).
The inherited firmware installation instructions are in [USAGE.md](USAGE.md).

## Prerequisites

In order to build Limine, the following programs have to be installed:
common UNIX tools (also known as `coreutils`),
`GNU make`, `grep`, `sed`, `find`, `awk`, `nasm`, `mtools`
(optional, necessary to build `limine-uefi-cd.bin`).
Furthermore, `gcc` or `llvm/clang` must also be installed, alongside
the respective binutils.

## Configure

If using a Limine-Eucleia source archive, run `./configure` directly.
Upstream Limine archives do not include Eucleia's graphical menu.

If checking out from the repository, run `./bootstrap` first in order to
download the necessary [dependencies](3RDPARTY.md) and generate the configure
script (`git`, `patch` and `GNU autoconf` required, plus `GNU automake` for
autoconf releases that do not install the auxiliary files themselves).

`./configure` takes arguments and environment variables; for more information
on these, run `./configure --help`.

> **NOTE:** `./configure` by default does not build any Limine port. Make sure
> to read the output of `./configure --help` and enable any or all ports!

Limine supports both in-tree and out-of-tree builds. Simply run the `configure`
script from the directory you wish to execute the build in. The following
`make` commands are supposed to be run inside the build directory.

## Building

To build Limine, run:

```bash
make    # (or gmake where applicable)
```

## Installing

This step will install Limine files to `share` and `bin` directories in the
specified prefix (default is `/usr/local`, see `./configure --help`).

To install Limine, run:

```bash
make install    # (or gmake where applicable)
```

## Eucleia theme and release packages

The graphical menu requires a theme pack and `eucleia.conf`. Build the curated
pack with `make theme` (Python 3, Pillow and FontTools required), or use the
pack from an Eucleia binary release. `make dist-binary` includes it together
with firmware, configuration and required notices. The loader build itself
does not require Python or the original design workspace.

`make install` installs firmware, the host tool and documentation into the
configured prefix; it does not enable the GUI or deploy a theme to an EFI
partition. See [deployment](docs/DEPLOYMENT.md) and
[release preparation](docs/RELEASE.md).
