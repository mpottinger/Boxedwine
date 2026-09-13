/* Opt-in compatibility for the owned, unmodified English Petz II executable.
 * Retail FUN_0047c82f (0x0047c82f), call at 0x0047c919, initializes only
 * cbSize/fMask of MENUITEMINFOA. MIIM_TYPE also requests text, so Wine 6
 * writes through the uninitialized dwTypeData (observed as 0x00000006).
 * The two recovered metadata-only call sites are normalized below. All other
 * requests retain Wine behavior. This is an application compatibility repair,
 * not an x86 instruction fix.
 */
#include <windows.h>

typedef BOOL (WINAPI *get_menu_info_fn)(HMENU, UINT, BOOL, LPMENUITEMINFOA);
static get_menu_info_fn original;
static void diagnostic(const char *s, DWORD n) { DWORD written; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, n, &written, NULL); }
#define DIAG(s) diagnostic(s, sizeof(s)-1)
__declspec(dllexport) volatile LONG DogzMenuRepairs;

/* Retail FUN_0047c82f (0x0047c82f), return 0x0047c91f, and
 * FUN_0047c9ad (0x0047c9ad), return 0x0047ca2a, only initialize cbSize
 * and fMask. Both get display text separately with GetMenuStringA.
 * Normalize their unused text destination even if the garbage pointer happens
 * to address writable memory; probing alone allowed stack corruption.
 */
static ULONG_PTR image_base;
static BOOL installed;
static BOOL WINAPI compatible_get_menu_info(HMENU menu, UINT item, BOOL bypos,
                                            LPMENUITEMINFOA info)
{
    ULONG_PTR caller = (ULONG_PTR)__builtin_return_address(0) - image_base;
    if (info && info->cbSize == 44 &&
        ((caller == 0x7c91f && info->fMask == 0x1b) ||
         (caller == 0x7ca2a && info->fMask == 0x3b))) {
        info->dwTypeData = NULL;
        info->cch = 0;
        if (InterlockedIncrement(&DogzMenuRepairs) == 1) DIAG("Dogz menu metadata request repaired\n");
    }
    return original(menu, item, bypos, info);
}

static BOOL equal_ascii(const char *a, const char *b)
{
    for (;;) {
        char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return FALSE;
        if (!x) return TRUE;
    }
}

BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID reserved)
{
    char path[MAX_PATH], *name;
    BYTE *base;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_IMPORT_DESCRIPTOR *imports;
    DWORD length;
    (void)dll; (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!length || length >= sizeof(path)) return TRUE;
    name = path;
    for (char *p = path; *p; p++) if (*p == '\\' || *p == '/') name = p + 1;
    if (!equal_ascii(name, "Petz II.exe")) return TRUE;
    original = (get_menu_info_fn)GetProcAddress(GetModuleHandleA("user32.dll"), "GetMenuItemInfoA");
    if (!original) return TRUE;
    base = (BYTE *)GetModuleHandleA(NULL);
    image_base = (ULONG_PTR)base;
    dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return TRUE;
    nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return TRUE;
    imports = (IMAGE_IMPORT_DESCRIPTOR *)(base +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imports->Name; imports++) {
        IMAGE_THUNK_DATA32 *thunk;
        if (!equal_ascii((char *)(base + imports->Name), "user32.dll")) continue;
        thunk = (IMAGE_THUNK_DATA32 *)(base + imports->FirstThunk);
        for (; thunk->u1.Function; thunk++) {
            DWORD old, ignored;
            const BYTE *measure = base + 0x7c919, *draw = base + 0x7ca24;
            if (thunk->u1.Function != (DWORD)(ULONG_PTR)original) continue;
            if (nt->OptionalHeader.SizeOfImage <= 0x7ca2a ||
                measure[0] != 0xff || measure[1] != 0x15 ||
                draw[0] != 0xff || draw[1] != 0x15 ||
                *(const DWORD *)(measure + 2) != (DWORD)(ULONG_PTR)&thunk->u1.Function ||
                *(const DWORD *)(draw + 2) != (DWORD)(ULONG_PTR)&thunk->u1.Function) {
                DIAG("Dogz menu compatibility: unsupported executable layout\n");
                return TRUE;
            }
            if (!VirtualProtect(&thunk->u1.Function, sizeof(DWORD), PAGE_READWRITE, &old)) return TRUE;
            thunk->u1.Function = (DWORD)(ULONG_PTR)compatible_get_menu_info;
            installed = TRUE;
            DIAG("Dogz menu hook installed\n");
            VirtualProtect(&thunk->u1.Function, sizeof(DWORD), old, &ignored);
            return TRUE;
        }
    }
    return TRUE;
}

/* WH_GETMESSAGE loads this DLL through Wine's supported GUI hook mechanism. */
__declspec(dllexport) LRESULT CALLBACK DogzLoadHook(int code, WPARAM wparam, LPARAM lparam)
{
    char event_name[] = "Local\\DogzMenuReady-00000000";
    DWORD pid = GetCurrentProcessId();
    HANDLE ready;
    for (int i = 0; i < 8; i++) event_name[20+i] = "0123456789abcdef"[(pid >> ((7-i)*4)) & 15];
    if (!installed) return CallNextHookEx(NULL, code, wparam, lparam);
    ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, event_name);
    if (ready) { SetEvent(ready); CloseHandle(ready); }
    return CallNextHookEx(NULL, code, wparam, lparam);
}
