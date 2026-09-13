#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "$0")" && pwd)
build_dir=${1:?Usage: build.sh /absolute/output/directory}
mkdir -p -- "$build_dir"
flags=(-std=c99 -Os -Wall -Wextra -Werror -Wno-cast-function-type -Wno-missing-field-initializers -nostdlib -fno-builtin -Wl,--no-insert-timestamp)
i686-w64-mingw32-gcc "${flags[@]}" -shared -Wl,--entry,_DllMain@12 \
    -o "$build_dir/dogz2-menu-compat.dll" "$source_dir/menu_compat.c" -lkernel32 -luser32
i686-w64-mingw32-gcc "${flags[@]}" -Wl,--entry,_entry@0 \
    -o "$build_dir/dogz2-browser.exe" "$source_dir/launch.c" -lkernel32 -luser32 -ladvapi32
i686-w64-mingw32-gcc "${flags[@]}" -Wl,--entry,_test_entry@0 \
    -o "$build_dir/dogz2-menu-test.exe" "$source_dir/menu_test.c" -lkernel32 -luser32
# The web filesystem has only a dummy imaadp32.acm. Build the actual Wine codec.
i686-w64-mingw32-gcc -Os -DNDEBUG -shared -Wl,--no-insert-timestamp \
    -I"$source_dir/wine/include" -o "$build_dir/imaadp32.acm" \
    "$source_dir/wine/imaadp32.c" "$source_dir/wine/imaadp32.def" -lwinmm
i686-w64-mingw32-gcc "${flags[@]}" -Wl,--entry,_audio_test_entry@0 \
    -o "$build_dir/dogz2-audio-test.exe" "$source_dir/audio_test.c" -lkernel32 -lmsacm32
