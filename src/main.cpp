// ChildUsageTracker — silent background process
// Monitors game session times and syncs a rolling JSON log to a GitHub Gist.
//
// Usage:
//   ChildUsageTracker.exe             — start tracking (no window, no console)
//   ChildUsageTracker.exe /?          — show this help
//   ChildUsageTracker.exe /install    — register as auto-start on Windows login
//   ChildUsageTracker.exe /uninstall  — remove auto-start entry

#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "config.h"
#include "dialogs.h"
#include "gist_client.h"
#include "process_monitor.h"
#include "tracker.h"

// ── constants ─────────────────────────────────────────────────────────────────

static constexpr wchar_t MUTEX_NAME[]  = L"ChildUsageTrackerMutex_v1";
static constexpr wchar_t REG_KEY[]     = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static constexpr wchar_t REG_VALUE[]   = L"ChildUsageTracker";
static constexpr char    GIST_FILE[]   = "child_usage_log.json";
static constexpr char    GIST_DESC[]   = "Child PC Usage Tracker Log";

// ── globals ───────────────────────────────────────────────────────────────────

static volatile bool g_running = true;

// ── helpers ───────────────────────────────────────────────────────────────────

static std::wstring GetExePath() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}

static std::wstring GetExeDir() {
    std::wstring path = GetExePath();
    const auto pos = path.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? path.substr(0, pos) : L".";
}

static void WriteLocalBackup(const std::wstring& dir, const std::string& json_str) {
    const std::wstring path = dir + L"\\sessions.json";
    std::ofstream f(path);
    if (f.is_open()) f << json_str;
}

// ── registry helpers ──────────────────────────────────────────────────────────

static void RegisterAutoStart() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;
    const std::wstring exePath = GetExePath();
    RegSetValueExW(hKey, REG_VALUE, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(exePath.c_str()),
                   static_cast<DWORD>((exePath.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
}

static void UnregisterAutoStart() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;
    RegDeleteValueW(hKey, REG_VALUE);
    RegCloseKey(hKey);
}

// ── shutdown handler (handles system shutdown / logoff) ───────────────────────

static BOOL WINAPI CtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_SHUTDOWN_EVENT ||
        dwCtrlType == CTRL_LOGOFF_EVENT   ||
        dwCtrlType == CTRL_CLOSE_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

// ── system-tray balloon notification (fire-and-forget) ───────────────────────

static void ShowTrayNotification(const wchar_t* title,
                                  const wchar_t* message,
                                  bool isError = false) {
    std::thread([t = std::wstring(title), m = std::wstring(message), isError]() {
        // Register a minimal window class (failure = already registered, fine).
        WNDCLASSEXW wc   = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"CUT_NotifyWnd";
        RegisterClassExW(&wc);

        // Message-only window — no taskbar entry, fully invisible.
        HWND hwnd = CreateWindowExW(0, L"CUT_NotifyWnd", L"", 0,
                                    0, 0, 0, 0,
                                    HWND_MESSAGE, nullptr,
                                    GetModuleHandleW(nullptr), nullptr);
        if (!hwnd) return;

        NOTIFYICONDATAW nid  = {};
        nid.cbSize           = sizeof(nid);
        nid.hWnd             = hwnd;
        nid.uID              = 1;
        nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        nid.uCallbackMessage = WM_USER + 1;
        nid.hIcon            = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(nid.szTip, L"ChildUsageTracker");
        Shell_NotifyIconW(NIM_ADD, &nid);

        // Populate balloon (truncate to field size limits).
        nid.uFlags      = NIF_INFO;
        nid.dwInfoFlags = isError ? NIIF_ERROR : NIIF_INFO;
        const std::wstring ti = t.substr(0, 63);
        const std::wstring mi = m.substr(0, 255);
        wcscpy_s(nid.szInfoTitle, ti.c_str());
        wcscpy_s(nid.szInfo,      mi.c_str());
        Shell_NotifyIconW(NIM_MODIFY, &nid);

        Sleep(12000); // keep icon alive long enough for balloon to display

        Shell_NotifyIconW(NIM_DELETE, &nid);
        DestroyWindow(hwnd);
    }).detach();
}

// ── worker thread ─────────────────────────────────────────────────────────────

static void WorkerThread(std::wstring exeDir) {
    // ── load config ───────────────────────────────────────────────────────────
    const std::wstring configPath = exeDir + L"\\config.ini";
    Config config;

    if (!config.load(configPath)) {
        // No config found — seed defaults and fall through to setup dialog.
        config.set("github",   "token",                 "");
        config.set("github",   "gist_id",               "");
        config.set("settings", "sync_mode",             "");
        config.set("settings", "poll_interval_seconds", "10");
        config.set("settings", "sync_interval_minutes", "5");
        config.set("games",    "cs2.exe",               "Counter-Strike 2");
        config.set("games",    "ScooterFlow.exe",        "ScooterFlow");
        config.save();
    }

    std::string token    = config.get("github",   "token",     "");
    std::string syncMode = config.get("settings", "sync_mode", "");

    // First-run setup: token not configured yet.
    if (token.empty() || token == "YOUR_GITHUB_PAT_HERE") {
        std::wstring pat;
        const auto choice = ShowSetupDialog(pat);
        if (choice == SetupChoice::Cancel) return;

        if (choice == SetupChoice::GitHub) {
            // Convert wide PAT to UTF-8 (PAT is always ASCII, but use proper API).
            const int len = WideCharToMultiByte(CP_UTF8, 0, pat.c_str(), -1,
                                                nullptr, 0, nullptr, nullptr);
            std::string narrow(len > 0 ? len - 1 : 0, '\0');
            if (len > 0)
                WideCharToMultiByte(CP_UTF8, 0, pat.c_str(), -1,
                                    narrow.data(), len, nullptr, nullptr);
            token    = std::move(narrow);
            syncMode = "gist";
            config.set("github", "token", token);
        } else {
            syncMode = "local";
        }
        config.set("settings", "sync_mode", syncMode);
        config.save();
    } else if (syncMode.empty()) {
        // Legacy config without sync_mode — assume gist (backward compat).
        syncMode = "gist";
        config.set("settings", "sync_mode", "gist");
        config.save();
    }

    const bool isLocalMode = (syncMode == "local");

    int pollSeconds = 10;
    int syncMinutes = 5;
    try {
        pollSeconds = std::stoi(config.get("settings", "poll_interval_seconds", "10"));
        syncMinutes = std::stoi(config.get("settings", "sync_interval_minutes", "5"));
    } catch (...) {}

    // ── build tracked-game map ────────────────────────────────────────────────
    const auto games_section = config.getSection("games");
    const int  gameCount     = static_cast<int>(games_section.size());
    Tracker tracker;
    tracker.setGames(games_section);

    // ── Gist / local storage setup ────────────────────────────────────────────
    GistClient gist(token);
    std::string gist_id = config.get("github", "gist_id", "");

    if (isLocalMode) {
        // Load history from local sessions.json.
        const std::wstring sessPath = exeDir + L"\\sessions.json";
        std::ifstream sf(sessPath);
        if (sf.is_open()) {
            try {
                std::ostringstream oss;
                oss << sf.rdbuf();
                tracker.loadFromJson(nlohmann::json::parse(oss.str()));
            } catch (...) {}
        }
        const std::wstring msg =
            L"Tracking " + std::to_wstring(gameCount) + L" game(s).\n"
            + std::to_wstring(tracker.completedSessionCount())
            + L" sessions loaded. Data saved locally only (sessions.json).";
        ShowTrayNotification(L"ChildUsageTracker \u2014 Running (local)", msg.c_str());

    } else if (gist_id.empty()) {
        // First Gist run: create it and persist the ID.
        nlohmann::json empty;
        empty["version"]         = "1.0";
        empty["sessions"]        = nlohmann::json::array();
        empty["active_sessions"] = nlohmann::json::object();
        empty["daily_totals"]    = nlohmann::json::object();

        gist_id = gist.create(GIST_DESC, GIST_FILE, empty.dump(2));
        if (!gist_id.empty()) {
            config.set("github", "gist_id", gist_id);
            config.save();
            const std::wstring url = L"https://gist.github.com/"
                                   + std::wstring(gist_id.begin(), gist_id.end());
            const std::wstring msg = L"Tracking " + std::to_wstring(gameCount) + L" game(s).\n"
                                   + L"Private Gist created:\n" + url;
            ShowTrayNotification(L"ChildUsageTracker \u2014 Started", msg.c_str());
        } else {
            ShowTrayNotification(
                L"ChildUsageTracker \u2014 Gist Error",
                L"Could not create GitHub Gist.\n"
                L"Check your token and internet connection.\n"
                L"Tracking locally only (sessions.json).",
                true);
        }
    } else {
        // Subsequent Gist run: restore history.
        const std::string content = gist.fetch(gist_id, GIST_FILE);
        if (!content.empty()) {
            try {
                tracker.loadFromJson(nlohmann::json::parse(content));
            } catch (...) {}
        }
        const std::wstring msg =
            L"Tracking " + std::to_wstring(gameCount) + L" game(s).\n"
            + std::to_wstring(tracker.completedSessionCount())
            + L" previous sessions loaded from Gist.";
        ShowTrayNotification(L"ChildUsageTracker \u2014 Running", msg.c_str());
    }

    // Force a sync shortly after startup to confirm connectivity.
    auto lastSync = std::chrono::steady_clock::now()
                  - std::chrono::minutes(syncMinutes);

    // ── main poll loop ────────────────────────────────────────────────────────
    while (g_running) {
        tracker.update(GetRunningProcessNames());

        const auto now = std::chrono::steady_clock::now();
        const auto minsElapsed =
            std::chrono::duration_cast<std::chrono::minutes>(now - lastSync).count();

        if (minsElapsed >= syncMinutes) {
            const std::string json_str = tracker.toJson().dump(2);
            if (!isLocalMode && !gist_id.empty())
                gist.update(gist_id, GIST_FILE, json_str);
            WriteLocalBackup(exeDir, json_str);
            tracker.clearDirty();
            lastSync = now;
        }

        // Sleep in 1-second increments so we react to g_running = false quickly.
        for (int i = 0; i < pollSeconds && g_running; ++i)
            Sleep(1000);
    }

    // ── final sync on clean shutdown ──────────────────────────────────────────
    {
        const std::string json_str = tracker.toJson().dump(2);
        if (!isLocalMode && !gist_id.empty())
            gist.update(gist_id, GIST_FILE, json_str);
        WriteLocalBackup(exeDir, json_str);
    }
}

// ── entry point ───────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE /*hInstance*/,
                   HINSTANCE /*hPrevInstance*/,
                   LPSTR     /*lpCmdLine*/,
                   int       /*nCmdShow*/) {
    // ── single-instance guard ─────────────────────────────────────────────────
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // ── parse command-line flags (use wide version for correctness) ───────────
    const std::wstring cmdLine = GetCommandLineW();

    // /? or --help or -h
    if (cmdLine.find(L"/?")     != std::wstring::npos ||
        cmdLine.find(L"--help") != std::wstring::npos ||
        cmdLine.find(L"-h")     != std::wstring::npos) {
        MessageBoxW(nullptr,
                    L"ChildUsageTracker — silent game-time tracker\n"
                    L"\n"
                    L"Usage:\n"
                    L"  ChildUsageTracker.exe             Start tracking (background, no window)\n"
                    L"  ChildUsageTracker.exe /?          Show this help\n"
                    L"  ChildUsageTracker.exe /install    Register auto-start on Windows login\n"
                    L"  ChildUsageTracker.exe /uninstall  Remove auto-start entry\n"
                    L"\n"
                    L"Configuration:\n"
                    L"  config.ini  — place in the same folder as the .exe\n"
                    L"  [games]     — add exe=Display Name lines to track games\n"
                    L"  [settings]  — poll_interval_seconds, sync_interval_minutes\n"
                    L"  [github]    — token (PAT with gist scope), gist_id\n"
                    L"\n"
                    L"Data is stored in sessions.json (local mode) or a private\n"
                    L"GitHub Gist (configured on first run).\n"
                    L"\n"
                    L"Dashboard: open dashboard/index.html in any browser.",
                    L"ChildUsageTracker — Help",
                    MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        return 0;
    }

    if (cmdLine.find(L"/install") != std::wstring::npos) {
        RegisterAutoStart();
        MessageBoxW(nullptr,
                    L"ChildUsageTracker has been registered.\n"
                    L"It will start automatically on Windows login.",
                    L"ChildUsageTracker \u2014 Installed",
                    MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        return 0;
    }

    if (cmdLine.find(L"/uninstall") != std::wstring::npos) {
        UnregisterAutoStart();
        MessageBoxW(nullptr,
                    L"ChildUsageTracker has been removed from auto-start.",
                    L"ChildUsageTracker \u2014 Uninstalled",
                    MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        return 0;
    }

    // ── install shutdown / logoff handler ─────────────────────────────────────
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    // ── show install prompt if not registered in auto-start ───────────────────
    {
        HKEY hKey = nullptr;
        bool isRegistered = false;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            wchar_t val[MAX_PATH] = {};
            DWORD sz = sizeof(val);
            isRegistered = (RegQueryValueExW(hKey, REG_VALUE, nullptr, nullptr,
                                             reinterpret_cast<BYTE*>(val), &sz) == ERROR_SUCCESS);
            RegCloseKey(hKey);
        }
        if (!isRegistered) {
            const auto choice = ShowInstallPrompt();
            if (choice == InstallChoice::Cancel) {
                CloseHandle(hMutex);
                return 0;
            }
            if (choice == InstallChoice::Install)
                RegisterAutoStart();
        }
    }

    // ── run worker and wait ───────────────────────────────────────────────────
    // No window or message pump is created — the process stays alive only
    // because the worker thread is running. It is invisible in the taskbar
    // and has no console window (/SUBSYSTEM:WINDOWS via WIN32_EXECUTABLE).
    const std::wstring exeDir = GetExeDir();
    std::thread worker(WorkerThread, exeDir);
    worker.join();

    CloseHandle(hMutex);
    return 0;
}
