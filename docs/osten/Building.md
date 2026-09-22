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

On a Linux host with `qemu-system-x86_64` and `socat`, capture the display
after three minutes without needing a graphical desktop on the host:

```sh
bash build/osten/tools/boot-smoke generated.x86_64/haiku-nightly.image boot-evidence
```

The helper records the first-boot Welcome screen, presses its default
"Try it out" action, and captures `boot-evidence/screenshot.ppm` after another
90 seconds. Inspect the latter image to confirm the Haiku desktop appeared.
A screenshot file alone does not establish a successful desktop boot.

## Persistent baseline build

The [baseline image workflow](../../.github/workflows/baseline-image.yml)
runs the same build on Ubuntu 24.04 when that workflow or the download helper
changes on `osten/main`. It can also be started manually from GitHub Actions.
Successful runs attach the raw image and a manifest with the OSTen and
buildtools commits. The artifact expires after seven days; the workflow is
the reproducible record. A successful image build still needs a QEMU boot
check before the baseline milestone is accepted.

The first accepted baseline and its captured desktop are recorded in
[Baseline.md](Baseline.md).

The [baseline boot screenshot workflow](../../.github/workflows/baseline-boot.yml)
waits for the image build, starts its image in QEMU, and attaches a screenshot
and QEMU logs. Inspect the screenshot before recording the boot milestone as
complete. Updating that workflow or the boot helper starts another check.

## First OSTen preview image

The `nightly-osten-raw` profile reuses the tested nightly build settings and
creates `osten-preview.image`. It adds an ordinary `System Folder` at the root
of the boot volume with `Control Panels`, `Extensions`, `Fonts`, and
`Startup Items`, plus a root `Applications` folder. The visible `System` and
`Finder` applications in `System Folder` select the OSTen shell at login.
Tracker and Deskbar remain available as recovery fallbacks but are not started
when those OSTen components are present. `System` supplies the global menu bar;
`Finder` owns the desktop, displays the OSTen boot disk and Trash, and opens
folders in separate spatial windows.

The first shell replacement and its QEMU verification are recorded in
[Shell.md](Shell.md).

From `generated.x86_64`, build this separate profile with:

```sh
PATH="$(cd ../build/osten/tools && pwd):$(cd ../../host-tools/bin && pwd):$PATH" \
  jam -q -j4 -sHAIKU_REVISION=osten-preview-e5a43367 @nightly-osten-raw
```

The [OSTen preview image workflow](../../.github/workflows/osten-preview.yml)
builds and boots that profile independently, saving the image, manifest, and
boot screenshots. The profile's visible folder layout must be checked in the
booted image before marking that part complete.


The `buildtools` checkout and `host-tools` directory live outside this
repository; do not commit generated binaries or disk images to Git.
