# OSTen

**The classic Macintosh road not taken.**

> **What if Apple had acquired BeOS instead of NeXT?**

OSTen explores an alternate progression of the classic Macintosh operating system: the familiar, direct, spatial experience of **Mac OS 8**, evolving on a technical foundation inspired by **BeOS** rather than the NeXT-derived path that led to Mac OS X.

It's a design thought experiment, not a claim about what Apple would actually have built. The goal is to carry the classic Mac approach forward without preserving the limitations of the original system.

OSTen is an independent, open-source operating-system project **built on Haiku**, the modern open-source successor to the BeOS tradition. Haiku supplies the underlying system foundation; OSTen develops its own Mac-inspired desktop, Finder, system interface, and behavior. It is not an Apple product or a skin for macOS.

## Status: early alpha

**OSTen boots as a development preview in x86-64 QEMU.** The desktop is real and under active development, but this is **not a stable release** or a recommendation for daily use.

### Present in the development preview

- **OSTen System:** a global menu bar with classic Mac-style commands, application switching, clock, and system controls.
- **OSTen Finder:** spatial folder windows, desktop icons, and file operations.
- **Classic window behavior:** custom window decoration, WindowShade, and dockable Finder pop-up folder tabs.
- **Desktop artwork:** custom hard disk and empty/full Trash icons.
- **System Folder:** a visible system directory, with an explicit **Special → System Folder Access…** lock/unlock mechanism in Finder.
- **Control Strip:** a collapsible accessory above the Finder pop-up tabs, with its own `Control Strip Modules` folder. **The current module controls are placeholders**, not finished system integrations.

### Still being developed

- Working Control Strip modules for sound, display, networking, power, Bluetooth, storage, and settings.
- External module discovery and loading.
- Complete, consistent desktop keyboard shortcuts and further Finder polish.
- Native applications, deeper system integration, testing, and release readiness.

This list describes the current direction, not a promise that unfinished features already work.

## Source and development

**The active OSTen development branch is [`osten/main`](https://github.com/Drmgiver/OSTen/tree/osten/main).** The repository's `master` branch is the Haiku baseline; it is **not** where OSTen desktop development takes place.

To check out the active code:

```bash
git clone --branch osten/main https://github.com/Drmgiver/OSTen.git
```

- [Build OSTen and run it in QEMU](https://github.com/Drmgiver/OSTen/blob/osten/main/docs/osten/Building.md) — includes historical baseline steps and the current OSTen preview profile.
- [OSTen preview builds and boot checks](https://github.com/Drmgiver/OSTen/actions/workflows/osten-preview.yml) — GitHub Actions produces development images and test evidence; artifacts are temporary.
- [Project charter and design direction](https://github.com/Drmgiver/OSTen/blob/osten/main/docs/osten/Project.md).

Please treat development previews as experimental, and report reproducible bugs with the build/commit used.

## Philosophy

The familiar parts of the classic Mac weren't just decorations. Spatial windows, direct manipulation, visible files, a coherent system menu, and a user-understandable System Folder were an approach to personal computing. OSTen aims to revisit that approach with a more modern technical foundation, not reproduce the old operating system byte-for-byte.

## Acknowledgements and licensing

OSTen stands on the work of the [Haiku project](https://www.haiku-os.org/) and the BeOS tradition. Apple's classic Macintosh system software is an important design inspiration. **OSTen is independent of and not endorsed by Apple, Haiku, or the former Be Inc.**

Inherited source code retains its applicable upstream licenses and notices. Check individual file headers and the [project charter](https://github.com/Drmgiver/OSTen/blob/osten/main/docs/osten/Project.md) for licensing intentions and attribution; the OSTen name and logo remain project identifiers.
