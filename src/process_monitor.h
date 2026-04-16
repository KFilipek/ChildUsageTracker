#pragma once
#include <set>
#include <string>

// Returns the lowercased basename of every running process (e.g. L"cs2.exe").
// Uses TlHelp32 snapshot — no extra libraries required beyond kernel32.
std::set<std::wstring> GetRunningProcessNames();
