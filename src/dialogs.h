#pragma once

// ── Commit 1: install prompt ──────────────────────────────────────────────────

// Shown when the exe is not registered in HKCU\...\Run.
// Returns the user's choice.
enum class InstallChoice { Install, RunOnce, Cancel };
InstallChoice ShowInstallPrompt();
