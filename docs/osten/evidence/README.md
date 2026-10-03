# OSTen verification evidence

October 3 evidence was captured from QEMU, inspected, and committed after
[verification run 37154388214](https://github.com/Drmgiver/OSTen/actions/runs/37154388214).
The image source and verification commits are distinct: the later commits
changed workflow and input helpers while leaving the runtime sources unchanged.
The verification workflow checked that relationship before reusing the image.

- `oct3-finder.png`: spatial boot-volume window.
- `oct3-context-menu.png`: right-click contextual menu.
- `oct3-transfer-tests.png`: Applications window and all-16-tests PASS dialog.
- `oct3-transfer-results.txt`: native test output from the serial log.
- `oct3-build-manifest.txt`: exact image/toolchain commits and run identifiers.
- `oct3-baseline.png`: freshly built stock Haiku fallback desktop.

Screenshots are verification evidence. Embedded component artwork retains its
original licenses; this record does not relicense inherited Haiku assets.
