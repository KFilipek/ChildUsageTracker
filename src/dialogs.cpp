#include "dialogs.h"
#include <windows.h>
#include <commctrl.h>

// Idempotent — safe to call multiple times.
static void EnsureCommonControls() {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);
}

// ── Install prompt ────────────────────────────────────────────────────────────

InstallChoice ShowInstallPrompt() {
    EnsureCommonControls();

    static const TASKDIALOG_BUTTON btns[] = {
        { 100, L"Install\nRegister to start automatically on every Windows login" },
        { 101, L"Run once\nTrack only this session without installing" },
    };

    TASKDIALOGCONFIG tdc   = {};
    tdc.cbSize             = sizeof(tdc);
    tdc.hInstance          = GetModuleHandleW(nullptr);
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    tdc.pszWindowTitle     = L"ChildUsageTracker";
    tdc.pszMainInstruction = L"How would you like to run ChildUsageTracker?";
    tdc.pszContent         =
        L"The tracker is not yet registered to start automatically with Windows.";
    tdc.pszMainIcon        = TD_SHIELD_ICON;
    tdc.dwCommonButtons    = TDCBF_CANCEL_BUTTON;
    tdc.pButtons           = btns;
    tdc.cButtons           = 2;
    tdc.nDefaultButton     = 100;

    int nButton = IDCANCEL;
    TaskDialogIndirect(&tdc, &nButton, nullptr, nullptr);

    if (nButton == 100) return InstallChoice::Install;
    if (nButton == 101) return InstallChoice::RunOnce;
    return InstallChoice::Cancel;
}

// ── PAT input window (internal helper) ───────────────────────────────────────

static constexpr wchar_t PAT_WND_CLASS[] = L"CUT_PATInputWnd";

struct PATState {
    HWND         hEdit    = nullptr;
    std::wstring result;
    bool         accepted = false;
};

static LRESULT CALLBACK PATWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* s = reinterpret_cast<PATState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        s = reinterpret_cast<PATState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));

        HFONT hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        // Helper to create and font a child control.
        auto mk = [&](DWORD exStyle, LPCWSTR cls, LPCWSTR txt, DWORD style,
                      int x, int y, int w, int h, HMENU id) -> HWND {
            HWND hc = CreateWindowExW(exStyle, cls, txt,
                                      WS_CHILD | WS_VISIBLE | style,
                                      x, y, w, h, hwnd, id,
                                      GetModuleHandleW(nullptr), nullptr);
            SendMessageW(hc, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            return hc;
        };

        mk(0, L"STATIC",
           L"Enter your GitHub Personal Access Token (PAT).\n"
           L"Create one at github.com/settings/tokens  \u2014  scope: gist only.",
           SS_LEFT, 14, 14, 390, 38, nullptr);

        s->hEdit = mk(WS_EX_CLIENTEDGE, L"EDIT", L"",
                      WS_TABSTOP | ES_AUTOHSCROLL,
                      14, 60, 390, 26, reinterpret_cast<HMENU>(101));
        SendMessageW(s->hEdit, EM_SETLIMITTEXT, 255, 0);
        SetFocus(s->hEdit);

        mk(0, L"BUTTON", L"Connect",
           WS_TABSTOP | BS_DEFPUSHBUTTON,
           230, 100, 90, 28, reinterpret_cast<HMENU>(IDOK));
        mk(0, L"BUTTON", L"Cancel",
           WS_TABSTOP,
           328, 100, 76, 28, reinterpret_cast<HMENU>(IDCANCEL));
        return 0;
    }
    case WM_COMMAND:
        if (!s) return 0;
        if (LOWORD(wp) == IDOK) {
            wchar_t buf[256] = {};
            GetWindowTextW(s->hEdit, buf, 256);
            if (wcslen(buf) < 10) {
                MessageBoxW(hwnd,
                            L"Please enter a valid token (at least 10 characters).",
                            L"Invalid Token", MB_OK | MB_ICONWARNING);
                return 0;
            }
            s->result   = buf;
            s->accepted = true;
            DestroyWindow(hwnd);
        } else if (LOWORD(wp) == IDCANCEL) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static std::wstring ShowPATInputBox() {
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = PATWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = PAT_WND_CLASS;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc); // idempotent — ignore if already registered

    PATState state = {};
    const int W  = 420, H = 148;
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        PAT_WND_CLASS,
        L"ChildUsageTracker \u2014 GitHub Token Setup",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        (sw - W) / 2, (sh - H) / 2, W, H,
        nullptr, nullptr, GetModuleHandleW(nullptr), &state);

    if (!hwnd) return {};
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        // IsDialogMessageW handles Tab, Enter (IDOK), and Esc (IDCANCEL).
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return state.accepted ? state.result : L"";
}

// ── First-run setup dialog ────────────────────────────────────────────────────

SetupChoice ShowSetupDialog(std::wstring& outPat) {
    EnsureCommonControls();

    static const TASKDIALOG_BUTTON btns[] = {
        { 200, L"Sync to GitHub Gist  (recommended)\n"
               L"Access stats from any device via the web dashboard" },
        { 201, L"Save locally only\n"
               L"Data stays on this PC in sessions.json" },
    };

    TASKDIALOGCONFIG tdc   = {};
    tdc.cbSize             = sizeof(tdc);
    tdc.hInstance          = GetModuleHandleW(nullptr);
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    tdc.pszWindowTitle     = L"ChildUsageTracker \u2014 First Run Setup";
    tdc.pszMainInstruction = L"Where should session data be stored?";
    tdc.pszContent         =
        L"Session data can be synced to a private GitHub Gist "
        L"(accessible from any device) or kept locally on this PC only.";
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.dwCommonButtons    = TDCBF_CANCEL_BUTTON;
    tdc.pButtons           = btns;
    tdc.cButtons           = 2;
    tdc.nDefaultButton     = 200;

    int nButton = IDCANCEL;
    TaskDialogIndirect(&tdc, &nButton, nullptr, nullptr);

    if (nButton == 200) {
        outPat = ShowPATInputBox();
        if (outPat.empty()) return SetupChoice::Cancel;
        return SetupChoice::GitHub;
    }
    if (nButton == 201) return SetupChoice::LocalOnly;
    return SetupChoice::Cancel;
}
