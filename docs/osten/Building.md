# Building the initial OSTen baseline

The initial baseline is an unmodified Haiku build from this fork. The OSTen
desktop is not implemented yet. These instructions target a case-sensitive
Linux filesystem and x86-64 QEMU. See the inherited `ReadMe.Compiling.md` for
all supported hosts and targets.

## Host dependencies

On Ubuntu 24.04, install the host compiler, Git, and Haiku's prerequisites:

```sh
sudo apt-get install git build-essential bison flex texinfo autoconf automake \
  gawk nasm wget zip unzip xorriso mtools python3 pkg-config \
  zlib1g-dev libzstd-dev libgmp-dev libmpfr-dev libmpc-dev
```

For a graphical boot check, also install `qemu-system-x86` and `ovmf`.
On Ubuntu, `--no-install-recommends` keeps that emulator installation small.

## Host tools and cross-compiler

In the parent directory of the OSTen checkout:

```sh
git clone https://github.com/haiku/buildtools.git buildtools
cd buildtools/jam
make
mkdir -p ../../host-tools/bin
./jam0 -sBINDIR="$(cd ../.. && pwd)/host-tools/bin" install
```

The first toolchain bootstrap used buildtools commit
`8375c2dbeaf109c520798cb234d57f0895463201`. Record the exact buildtools
commit alongside the OSTen commit for each build.

In the OSTen checkout:

```sh
mkdir -p generated.x86_64
cd generated.x86_64
PATH="$(cd ../../host-tools/bin && pwd):$PATH" \
  MAKEFLAGS=-j4 ../configure \
  --cross-tools-source ../../buildtools --build-cross-tools x86_64
```

This first invocation builds a cross-compiler and can take substantial time.
Its output is under `generated.x86_64`, which Git ignores. Keep that directory
if you want later builds to reuse the compiler. Adjust `-j4` to available
memory and cores; parallel compilation has not yet been validated for OSTen.

## Image and boot check

GitHub's Haiku mirror used for this fork does not advertise `hrev` tags, so
Jam cannot derive the revision automatically from this shallow checkout. Pass
an explicit build label with Jam's `-s` option. This label describes the fork
baseline rather than claiming an upstream Haiku revision. Record the source
commit alongside it, and replace the label when an upstream revision can be
established from tags.

From `generated.x86_64`:

```sh
PATH="$(cd ../../host-tools/bin && pwd):$PATH" \
  jam -q -j4 -sHAIKU_REVISION=osten-baseline-e5a43367 @nightly-raw
```

If the HaikuPorts build-package host is unreachable but Haiku's build-package
CDN is accessible, put OSTen's optional download helper before the host tools
on `PATH` for this Jam invocation:

```sh
PATH="$(cd ../build/osten/tools && pwd):$(cd ../../host-tools/bin && pwd):$PATH" \
  jam -q -j4 -sHAIKU_REVISION=osten-baseline-e5a43367 @nightly-raw
```

The helper only redirects build-package files; the repository index retains
its original checksum-specific URL. It requires Bash and `/usr/bin/wget` on
the Linux build host. On an unrestricted host, use the ordinary invocation.

The nightly raw target produces `haiku-nightly.image` in the build output.
The first milestone is to boot that stock image in QEMU and record the
versions, commits, command line, and observed desktop. Image naming and
contents will change when an OSTen image profile exists.

## Persistent baseline build

The [baseline image workflow](../../.github/workflows/baseline-image.yml)
runs the same build on Ubuntu 24.04 when that workflow or the download helper
changes on `osten/main`. It can also be started manually from GitHub Actions.
Successful runs attach the raw image and a manifest with the OSTen and
buildtools commits. The artifact expires after seven days; the workflow is
the reproducible record. A successful image build still needs a QEMU boot
check before the baseline milestone is accepted.

The `buildtools` checkout and `host-tools` directory live outside this
repository; do not commit generated binaries or disk images to Git.
