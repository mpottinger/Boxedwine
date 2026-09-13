# Dogz II browser compatibility and fault record

This branch is based on BoxedWine 26R1.0, commit
`d7d5a1421bd781a81cbdf8f222cced11a7ebd76e`. It runs an owned English retail
Dogz II installation inside browser Wine. No game files, serials, registry
contents, or adopted pets are distributed by this fork.

## 1. BoxedWine scheduler: guest fault escapes into JavaScript

**Observed:** opening Pick A Pet on the stock web release threw an uncaught
`WebAssembly.Exception` of C++ type `int`, freezing the emulator.

**Cause:** the release links with `-fwasm-exceptions`, but does not compile
`kscheduler.cpp` with that flag. Guest memory faults signal Wine and throw an
integer to leave the interpreter. The scheduler's catch must use WASM EH to
intercept it. Instruction fetch was also outside that catch boundary.

**Fix:** compile only the scheduler translation unit with `-fwasm-exceptions`
and move `getNextOp()` inside its existing try block. Recovery resets nextOp so
the next slice decodes Wine's signal handler. Link `-g2` retains function names
for useful future WASM stacks. No memory permissions are relaxed.

**Verified:** rebuilt browser runs delivered subsequent guest faults to Wine,
with no escaping JavaScript exception. Enabling EH globally instead regressed
the guest loader (`/bin/wine: file too short`); that experiment was removed.
The remaining interpreter intentionally retains the release compilation model.

## 2. Retail menu requests: uninitialized destination and stack corruption

**Observed:** after the scheduler repair, Wine reported a write to address 6
at `WideCharToMultiByte`, guest VA `0x7b060761`. A later invocation with a
writable garbage destination corrupted the stack and tried to execute
`0x20656874` (text bytes).

**Cause:** disassembly of the owned retail PE and its canonical Ghidra export
show two calls that initialize only MENUITEMINFOA.cbSize and fMask:

| Retail function | GetMenuItemInfoA call | Return | Size / mask |
|---|---|---|---|
| FUN_0047c82f, 0x0047c82f | 0x0047c919 | 0x0047c91f | 44 / 0x1b |
| FUN_0047c9ad, 0x0047c9ad | 0x0047ca24 | 0x0047ca2a | 44 / 0x3b |

MIIM_TYPE requests text as well as type. The text destination/capacity are
uninitialized. Both functions obtain display text separately using
GetMenuStringA; this call only supplies metadata/bitmaps. Wine 6's
[GetMenuItemInfo_common](https://github.com/wine-mirror/wine/blob/wine-6.0/dlls/user32/menu.c)
passes that garbage destination to its text conversion.

**Fix:** opt-in `tools/dogz2/menu_compat.c` sets the unused destination and
capacity to null/zero for only those two caller offsets, sizes and masks.
It validates their import-call layout before hooking the process IAT. Other
callers pass through to Wine. The PE file and assets on disk stay unchanged.
Checking pointer writability alone was insufficient and was removed.
This is an application compatibility repair, not an x86 opcode correction.

**Verified:** `menu_test.c` passed inside browser Wine using synthetic x86
callers at the recovered offsets and the real Wine menu API. It covers
address 6, writable garbage, an unrelated caller, and the modern structure
size. Valid ordinary requests still receive their text; defective metadata
requests no longer write through garbage buffers.

## 3. Wine web image: actual IMA ADPCM codec missing

**Observed:** Dogz's startup dialog said Unable To Decompress Wave Files,
error 512 (`ACMERR_NOTPOSSIBLE`).

**Cause:** the web filesystem's imaadp32.acm is a 1,032-byte dummy Wine PE.
Its implementation is absent from `/opt/wine/lib/wine`, and Drivers32 is
empty. Seeing the filename alone did not establish a working codec.
`CheckACMAvail` (retail 0x0048b823) requests IMA format 0x11, mono 22050 Hz,
512-byte blocks / 1017 samples, to unsigned 8-bit PCM. Runtime
`FUN_00476288` (0x00476288) also uses ACM to decompress real sound resources.

**Fix:** build the unmodified Wine 6 IMA codec as a native PE ACM, replace the
dummy file, and have the launcher register it and its native DLL override
before starting Dogz. This handles already-persisted browser registries.
Decoder source provenance, hashes, license, and build adaptation are in
[tools/dogz2/wine/README.md](../tools/dogz2/wine/README.md).

**Verified:** `audio_test.c` passed inside browser Wine: format suggestion,
stream creation and conversion produced 1017 PCM samples, each checked against
an analytic IMA ramp. Live playback also produced nonzero output in the running browser audio context.
The startup probe is not bypassed and sound is not disabled as a workaround.

## 4. Launcher and packaging constraints

Wine 6's [LoadAppInitDlls](https://github.com/wine-mirror/wine/blob/wine-6.0/dlls/kernelbase/loader.c)
is a stub. AppInit registry configuration was tried and removed. An initial
remote-memory loader also hit BoxedWine's unsupported ptrace path; it was
removed without adding debugger syscalls. The maintained launcher uses a
thread-specific WH_GETMESSAGE GUI hook to load the compatibility DLL, waits
for its readiness event, and keeps the DLL referenced until Dogz exits.

Bajoues.pet requests French Bouledogue; Chips.pet requests Scottish-terrier.
An English-only resource tree does not satisfy those saved breed names.
Package the corresponding owned French breed collection in addition to the
English files. This is an asset-packaging issue, not an emulator fault.

Root ZIP files take precedence over later ZIP overlays. Merge initial game
registry settings into the root filesystem. Preserve the installation's
per-app win98 setting (`IsWindowsNT`, retail 0x0048ab94). The game derives
its installed root in `CShlGlobals::LoadGlobalsNeededForSSV` (0x004adfb3) and
reads installation settings in `CDataFile::GetInstData` (0x004ac831).

## Build and reproduce

Use Emscripten 4.0.7 for the emulator, and MinGW i686 GCC for the compatibility
components and tests. Apply host-appropriate memory/CPU limits.

```sh
cd project/emscripten
EMCC_CORES=1 make -j1 BUILD_DIR=/absolute/build/directory SHELL_FILE=shell.html
cd ../..
tools/dogz2/build.sh /absolute/build/directory
```

An explicit BUILD_DIR bypasses the release wrapper's CPU-count parallelism.
Build all C++ objects again if global compilation flags change. Package the
owned game plus dogz2-browser.exe as the app ZIP, install dogz2-menu-compat.dll
at C:\, and the built imaadp32.acm in C:\windows\system32. Launch the wrapper
from the game's directory. The native-port repository's tools/browser_wine
contains the local packager and loopback server, with release checksum pins.

Final acceptance is live pet selection, animation and interaction, usable sound,
and saving/reopening. A splash screen or a visible empty playpen does not
establish that result. Live verification passed on 2026-09-13: Chips and Bajoues animate together;
toy interaction and toybox navigation work; the browser audio context is running
and produced nonzero output (peak 0.5257 over a 48-callback interaction sample).
Normal File → Exit, explicit IndexedDB flush, and a complete page reload
preserved SHA-256 hashes of both saved pets. The game was restarted afterward.
This establishes the exercised path, not exhaustive coverage of all features.

Run the standalone regression EXEs as separate app ZIPs against this same Wine
filesystem. `dogz2-menu-test.exe` needs no game assets. `dogz2-audio-test.exe`
needs the restored codec in system32; it loads that module and registers its
DriverProc locally for the test. Successful runs print MENU REGRESSION PASS
and ACM REGRESSION PASS respectively.

A later restart while the automation tab was hidden reported Wine's generic
"explorer process failed to start" desktop-driver warning. Both explorer.exe.so
and winex11.drv.so are present. A repeated visible run passed. Controlled
follow-up trials on 2026-09-13 held the tab hidden for 25 seconds immediately
after Showing Window, and 133 seconds after the first interpreter loop. Both
verified document.visibilityState was hidden and the log stayed unchanged; both
resumed and installed the compatibility hook without an Explorer warning or
loader timeout. The long trial visibly reached the welcome dialog. The earlier
warning remains unreproduced; visibility is not an established cause, and no
speculative driver or clock patch was added. The native checkout records both
trials in artifacts/browser_wine/startup-visibility-2026-09-13.json.

Additional live verification on 2026-09-13 used isolated browser root
`/dogz2-lifecycle-hidden-3`: adopted a Bouledogue through the naming and pledge
dialogs, returned to playpen, exited, flushed IndexedDB and reloaded. The new
`browsertest.pet` was 47,728 bytes; SHA-256 before and after reload was
`eab359b57037106b7048ee2f21194ad7c2deedc2ddaf0e886cafd2f6d6ac2d49`.
Restarting the game visibly loaded the adopted puppy in the playpen. This
extends verification to new-pet creation and persistence. Evidence:
`artifacts/browser_wine/adoption-verification-2026-09-13.json` and
`adoption-reopened-2026-09-13.png` in the native-port checkout.
Automated game controls used 120–180 ms presses; instantaneous clicks were
sometimes missed by the retail polling loop. No input patch was made.

Breed-selection verification on 2026-09-13: the retail adoption menu exposes
nine choices from the 18 packaged DOG files. Fresh screenshots show Bouledogue,
Bulldog, Chihuahua, Great Dane and Corniaud with their pets loaded in the
adoption center, without another crash or decompression dialog. This is five
observed selections, not a completed nine-entry sweep. Popup previews resize
and reposition menu rows; the game window also changed size during automation.
Those input observations are not established Wine defects and prompted no patch.
Evidence: `artifacts/browser_wine/breed-verification-2026-09-13.json` in the
native-port checkout. Matt reported "so far so good" and "it seems fine" during
this verification.

## Working-build handoff — 2026-09-13

The served JavaScript/WASM, packaged launcher, menu DLL and real codec were
compared against the tested build and match byte-for-byte. The packaged retail
PE matches the owned installation, all 18 DOG files are present, and the local
HTTP service is active. The fork's implementation is still 169b8a67; subsequent
commits document verification only. A fresh main-session screenshot shows both
pets in the playpen with a usable open Petz menu. Matt reports the game seems
fine. Menu regression, decoder regression, live sound/toys, new adoption,
save/reload and controlled startup-interruption evidence are recorded above.

The additional nine-entry breed sweep is retired rather than used as a new
product gate: five adoption choices and the saved Scottish-terrier have loaded,
and no new breed failure was observed. Do not claim every localized menu entry
was individually tested. The earlier desktop-driver warning remains a historical
unreproduced observation; it is not evidence for an additional clock or driver
patch. This handoff does not assert exhaustive coverage of every retail feature.
Native-checkout receipt: `artifacts/browser_wine/handoff-audit-2026-09-13.json`.
