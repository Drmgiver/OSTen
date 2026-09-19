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

## Host tools and cross-compiler

In the parent directory of the OSTen checkout:

```sh
git clone https://github.com/haiku/buildtools.git buildtools
cd buildtools/jam
make
mkdir -p ../../host-tools/bin
./jam0 -sBINDIR="$(cd ../.. && pwd)/host-tools/bin" install
```

The buildtools checkout should be recorded at a known commit for a repeatable
build. Record `git -C buildtools rev-parse HEAD` alongside the OSTen commit.

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
