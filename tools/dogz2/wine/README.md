# IMA ADPCM runtime component

The BoxedWine 26R1.0 web Wine 6 filesystem contains a 1,032-byte dummy
`windows/system32/imaadp32.acm`, without the corresponding implementation
under `/opt/wine/lib/wine`. Its Drivers32 registry section is empty. The
presence of the dummy filename is not evidence of a working decoder.

These unmodified Wine 6.0 source files are compiled as a native 32-bit PE ACM
with MinGW so the actual decoder can be installed in that filesystem:

- `imaadp32.c`: https://github.com/wine-mirror/wine/blob/wine-6.0/dlls/imaadp32.acm/imaadp32.c
  SHA-256 `3c58f2cb992f0e993d656bbeb432e0c917162cff86e4c3ef04a7d71148cb880e`
- `include/msacmdrv.h`: https://github.com/wine-mirror/wine/blob/wine-6.0/include/msacmdrv.h
  SHA-256 `404ae8d543767071ef64f74a19763e810d5b51545f86f28c9be22c09d04a88f3`

The LGPL 2.1-or-later license and authorship headers are retained;
`COPYING.LIB` accompanies the source. The local debug header removes only
Wine-specific logging and supplies its array-size macro. `imaadp32.def`
exports the same DriverProc entry point as Wine's `.spec`. The decoder's
algorithms and ACM dispatch are not changed. The normal MinGW CRT is linked.

The Dogz launcher registers `msacm.imaadpcm=imaadp32.acm` in the isolated
Wine Drivers32 key and selects this native codec before starting Dogz.
This also upgrades a browser whose registry was saved before the repair.

Dogz's `CheckACMAvail` (0x0048b823) requests format 0x11, mono 22,050 Hz,
512-byte blocks, 1,017 samples per block, converted to unsigned 8-bit PCM.
Without the codec, `acmFormatSuggest` returns 512 (`ACMERR_NOTPOSSIBLE`).
The runtime conversion in `FUN_00476288` (0x00476288) also uses ACM for the
actual sound resources. Merely bypassing the startup probe would not repair
playback.

Verification on 2026-09-13: `audio_test.c` passed in the same BoxedWine/Wine 6
browser runtime. It opened this codec through ACM, used Dogz's format-suggestion
flags `0xf0000`, decoded a 512-byte synthetic IMA block, and checked every one
of 1,017 output samples against an analytic ramp. This exercises format
suggestion, stream open, prepare, convert, unprepare and close. The first test
harness incorrectly supplied a DLL path to `ACM_DRIVERADDF_NAME` (which expects
a registry value name); that harness error was corrected to explicit module/
DriverProc registration. No codec change was needed for the passing test.
