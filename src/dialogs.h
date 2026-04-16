#pragma once
#include <string>

// ── Commit 1: install prompt ──────────────────────────────────────────────────

// Shown when the exe is not registered in HKCU\...\Run.
enum class InstallChoice { Install, RunOnce, Cancel };
InstallChoice ShowInstallPrompt();

// ── Commit 2: first-run setup ─────────────────────────────────────────────────

// Shown once when no storage mode has been configured yet.
// If GitHub is chosen, outPat is populated with the entered PAT.
enum class SetupChoice { GitHub, LocalOnly, Cancel };
SetupChoice ShowSetupDialog(std::wstring& outPat);
