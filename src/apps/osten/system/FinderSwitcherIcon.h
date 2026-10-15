/* SPDX-License-Identifier: MIT */
/* Finder face adapted from the image provided for OSTen's app selector.
 * 22x22 RGBA palette bitmap with antialiased transparency.
 * Stored in source so the classic Finder icon appears independently of
 * mime-database registrations. */
#ifndef OSTEN_FINDER_SWITCHER_ICON_H
#define OSTEN_FINDER_SWITCHER_ICON_H
static const int kOSTenFinderSwitcherSize = 22;
static const uint32 kOSTenFinderSwitcherPalette[] = {
0x00000000, 0xfefeff0a, 0xf8f9fa30, 0xf3f3f449, 0xd3d3d306, 0xf7f6f76e, 0xf4f5f693, 0xecf0f2ef,
0xe8e9ebf2, 0xe3e4e8cf, 0xe6e6e9ae, 0xd5d6dbcd, 0xd3d4d8fc, 0xe0e2e5a2, 0xe7e9ec6c, 0x92b8d5fa,
0xafcde4ee, 0xcee7f3fe, 0xd0dce6fe, 0xadc4d8f9, 0x7ab2d9fe, 0x64a6d9fe, 0x8d97a7fe, 0xc6c8cefd,
0xd7dae0c5, 0xaee2fafe, 0x8fd5fbfe, 0x72c6faff, 0x54b6faff, 0x39a4f9ff, 0x4b8cc3ff, 0xe1e3e55b,
0x85c7f0fe, 0x6bb8e9ff, 0x4b96d4ff, 0xaebbcafc, 0x5287b4ff, 0x437db1ff, 0x1c4e84ff, 0x295e95ff,
0x4baaefff, 0x3d92daff, 0x5bc0ffff, 0x2e6aa3ff, 0x378ed4ff, 0x479ee2ff, 0x3798e7ff, 0x8ea8c1ff,
0x9aa1affd, 0x979ba5fd, 0x666c78fb, 0x737884fd, 0x7c828dfe, 0x595d68fa, 0xb1b1b103, 0xaaaad406,
0x7f7fff02, 0x7f7f7f02
};
static const char* kOSTenFinderSwitcherRows[22] = {
"ABBBBCCDCCCCCCCDCCBBEB",
"ABBFFFGGFGHIHIIIIJKFBB",
"ABKGFFKKLMMMMMMMMMIINB",
"AOIKPQRSQTPPPUUUVVWXSD",
"AOYYVRHRZaaabbbccdeMRf",
"AONQgHRZaaaagbbhcciMMf",
"AOHjgRZakgaaaVlccciMMf",
"BKITgZaUmhabbimccciXMf",
"BJIThaaVnhbbbemccciTXN",
"BKHjhagbibbbboiccopjXY",
"BJIjobbhbbbhcqoocopjXN",
"CJIjobberVcoosnVcdpjXL",
"CKIjocccenrrrrioodijjL",
"BYIjscccccttoooddoiXjL",
"BYIMVtuuuuuuuuupiivXjN",
"BNSSISSQTTjjPPjjjjXjXO",
"BOMXjXMMMMMXXXXjvwwjLC",
"BBDXxyz0xx000zzyy10LCA",
"A2ANMjXXxxxwwWWWWWXNCB",
"AB2BNNNIMMMMMXXMYNNBAB",
"AAAAAABCDDffDFCBABCAAA",
"AAABB2EBBBAABBAE345BAA"
};
#endif
