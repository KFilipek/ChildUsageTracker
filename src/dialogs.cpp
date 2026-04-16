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
