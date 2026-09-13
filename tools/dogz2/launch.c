/* Wine 6 LoadAppInitDlls is a stub. An explicit GUI hook loads the opt-in
 * compatibility DLL into the owned, unchanged retail process. */
#include <windows.h>
void WINAPI entry(void)
{
    STARTUPINFOA startup = {sizeof(startup)};
    PROCESS_INFORMATION child;
    static char command[] = "\"Petz II.exe\"";
    char event_name[] = "Local\\DogzMenuReady-00000000";
    HMODULE dll;
    HHOOK hook = NULL;
    HANDLE ready = NULL;
    DWORD written, stage = 1;
    HKEY key;
    static const char codec[] = "imaadp32.acm", native[] = "native";
    /* Wine 6 web image omits the codec and its Drivers32 registration.
     * Register the real replacement before starting the ACM-using process,
     * including when this browser already has a persisted registry. */
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32",
        0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) ExitProcess(2);
    RegSetValueExA(key, "msacm.imaadpcm", 0, REG_SZ, (const BYTE *)codec, sizeof(codec));
    RegCloseKey(key);
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Wine\\DllOverrides",
        0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) ExitProcess(2);
    RegSetValueExA(key, codec, 0, REG_SZ, (const BYTE *)native, sizeof(native));
    RegCloseKey(key);
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &child)) ExitProcess(1);
    if (WaitForInputIdle(child.hProcess, 120000) != 0) goto failed;
    stage = 2;
    for (int i = 0; i < 8; i++) event_name[20+i] = "0123456789abcdef"[(child.dwProcessId >> ((7-i)*4)) & 15];
    ready = CreateEventA(NULL, TRUE, FALSE, event_name);
    dll = LoadLibraryA("C:\\dogz2-menu-compat.dll");
    if (!ready || !dll) goto failed;
    stage = 3;
    hook = SetWindowsHookExA(WH_GETMESSAGE, (HOOKPROC)GetProcAddress(dll, "DogzLoadHook@12"), dll, child.dwThreadId);
    if (!hook) goto failed;
    PostThreadMessageA(child.dwThreadId, WM_NULL, 0, 0);
    stage = 4;
    if (WaitForSingleObject(ready, 120000) != WAIT_OBJECT_0) goto failed;
    /* Keep the hook installed for the game's lifetime: it owns the DLL reference. */
    WaitForSingleObject(child.hProcess, INFINITE);
    UnhookWindowsHookEx(hook);
    CloseHandle(ready);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    ExitProcess(0);
failed:
    { char msg[] = "Dogz loader stage=0 error=00000000\n"; DWORD error = GetLastError();
      msg[18] = '0' + stage;
      for (int i=0;i<8;i++) msg[25+i] = "0123456789abcdef"[(error >> ((7-i)*4))&15];
      WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, sizeof(msg)-1, &written, NULL); }
    if (hook) UnhookWindowsHookEx(hook);
    TerminateProcess(child.hProcess, 1);
    ExitProcess(1);
}
