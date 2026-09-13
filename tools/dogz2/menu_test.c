/* Regression checks execute the real Wine menu API through synthetic x86
 * callers at the two recovered retail return offsets. No game data is needed. */
#include "menu_compat.c"

static BOOL call_from(ULONG_PTR offset, HMENU menu, MENUITEMINFOA *info)
{
    BYTE *code = (BYTE *)(image_base + offset - 27);
    DWORD args[] = {(DWORD)(ULONG_PTR)info, FALSE, 123, (DWORD)(ULONG_PTR)menu};
    for (int i = 0; i < 4; i++) {
        *code++ = 0x68;
        *(DWORD *)code = args[i]; code += 4;
    }
    *code++ = 0xb8;
    *(DWORD *)code = (DWORD)(ULONG_PTR)compatible_get_menu_info; code += 4;
    *code++ = 0xff; *code++ = 0xd0;
    *code = 0xc3;
    return ((BOOL (*)(void))(image_base + offset - 27))();
}

void WINAPI test_entry(void)
{
    MENUITEMINFOA info;
    char destination[32];
    HMENU menu = CreatePopupMenu();
    DWORD size, mask, written;
    int failed = 0;
    original = GetMenuItemInfoA;
    image_base = (ULONG_PTR)VirtualAlloc(NULL, 0x80000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!menu || !image_base || !AppendMenuA(menu, MF_STRING, 123, "Test pet")) ExitProcess(10);
    for (int test = 0; test < 4; test++) {
        ULONG_PTR offset = test == 1 ? 0x7ca2a : 0x7c91f;
        if (test == 2) offset += 1; /* An unrelated caller must retain text copy. */
        for (unsigned i = 0; i < sizeof(info); i++) ((BYTE *)&info)[i] = 0xa5;
        for (unsigned i = 0; i < sizeof(destination); i++) destination[i] = '!';
        size = test == 3 ? sizeof(info) : 44;
        mask = test == 1 ? 0x3b : 0x1b;
        info.cbSize = size;
        info.fMask = mask;
        info.dwTypeData = test == 0 ? (char *)6 : destination;
        info.cch = sizeof(destination);
        if (!call_from(offset, menu, &info) || info.wID != 123 || info.cbSize != size || info.fMask != mask) failed++;
        if (test < 2) {
            if (destination[0] != '!' || DogzMenuRepairs != test + 1) failed++;
        } else if (destination[0] != 'T' || destination[8] != 0 || DogzMenuRepairs != 2) failed++;
    }
    DestroyMenu(menu);
    VirtualFree((void *)image_base, 0, MEM_RELEASE);
    if (failed) {
        static const char message[] = "MENU REGRESSION FAIL\n";
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, sizeof(message)-1, &written, NULL);
    } else {
        static const char message[] = "MENU REGRESSION PASS: bad pointer, writable garbage, unrelated caller, modern structure\n";
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, sizeof(message)-1, &written, NULL);
    }
    ExitProcess(failed ? 1 : 0);
}
