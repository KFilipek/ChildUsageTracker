#include "process_monitor.h"

// windows.h must come before TlHelp32.h
#include <windows.h>
#include <TlHelp32.h>
#include <algorithm>
#include <cctype>

std::set<std::wstring> GetRunningProcessNames() {
    std::set<std::wstring> result;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            std::wstring name = entry.szExeFile;
            // Normalise to lowercase so comparisons are case-insensitive.
            std::transform(name.begin(), name.end(), name.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
            result.insert(std::move(name));
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}
