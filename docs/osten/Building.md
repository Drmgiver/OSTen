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
Jam cannot derive the revision automatically from a shallow checkout. Before
the first Jam invocation, create the ignored local file
`build/jam/UserBuildConfig` from the OSTen repository root with an explicit,
honest build label, for example:

```sh
printf '%s\n' 'HAIKU_REVISION = osten-baseline-e5a43367 ;' \
  > build/jam/UserBuildConfig
```

This label describes the fork baseline rather than claiming an upstream Haiku
revision. Replace it when the source revision can be established from tags.

From `generated.x86_64`:

```sh
PATH="$(cd ../../host-tools/bin && pwd):$PATH" jam -q -j4 @nightly-raw
```

The inherited target should produce `haiku.image` in the build output. The
first milestone is to boot that stock image in QEMU and record the versions,
commits, command line, and observed desktop. Image naming and contents will
change when an OSTen image profile exists.

The `buildtools` checkout and `host-tools` directory live outside this
repository; do not commit generated binaries or disk images to Git.
