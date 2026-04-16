# ChildUsageTracker

A lightweight Windows background process that silently tracks how long your child plays specific games. On first run it guides you through choosing between **syncing to a private GitHub Gist** (accessible from any device via the web dashboard) or **saving locally** to a JSON file. Comes with a **parent-only web dashboard** for charts, trends, and session history.

---

## Features

### Tracker (`ChildUsageTracker.exe`)
- **Completely invisible** — no window, no console, no taskbar entry (Win32 `SUBSYSTEM:WINDOWS`)
- **Single-instance guard** — named mutex prevents duplicate processes
- **Configurable game list** — any `.exe` name, no recompile needed
- **Process polling** — checks running processes every N seconds (default: 10 s)
- **Two storage modes:**
  - `gist` — pushes a rolling private JSON log to GitHub every N minutes
  - `local` — writes `sessions.json` next to the exe (no internet required)
- **Guided first-run setup** — interactive dialogs walk you through install and storage choice
- **Tray balloon notifications** — startup confirmation, Gist URL, config errors
- **Auto-start** — prompted on first launch; also available via `/install` / `/uninstall` flags
- **Clean shutdown sync** — final write on system shutdown / logoff

### Dashboard (`dashboard/index.html`)
- Open directly in any browser — no server, no build step
- **Load data from GitHub Gist** (paste URL or ID) or **from a local `sessions.json`** file
- **Timeframes:** 7 days · 30 days · 3 months · 1 year · all time
- **Charts:**
  - Daily playtime (stacked bar per game)
  - Game share (doughnut)
  - Avg session length per game
  - Play time by hour-of-day
  - Sessions per weekday (bar + line overlay)
- **Streaks & records** — longest/current streak per game, all-time top game, busiest day
- **Session table** — last 100 sessions with game badge, start/end time, duration
- **Live indicator** — animated banner when a session is currently active in the Gist

---

## Requirements

| Tool | Notes |
|------|-------|
| Windows 7+ | Runtime target (Windows 10/11 recommended) |
| [VS 2019 Build Tools](https://visualstudio.microsoft.com/vs/older-downloads/) | With the **C++ build tools** workload (`visualstudio2019-workload-vctools`) |
| CMake ≥ 3.16 | Bundled with VS Build Tools at `Common7\IDE\CommonExtensions\Microsoft\CMake` |
| Internet access | Only required for GitHub Gist mode |

> **No .NET, no Python, no Node.js required.**

---

## Build

```bat
git clone https://github.com/yourname/random-app.git
cd random-app
build.bat
```

Output: `build\ChildUsageTracker.exe`

The script initialises the VS 2019 x64 environment, runs CMake with Ninja, and downloads `nlohmann/json` automatically via `FetchContent`.

---

## First Run

When you launch `ChildUsageTracker.exe` for the first time two dialogs appear in sequence.

### 1. Install or run once?

| Choice | Effect |
|--------|--------|
| **Install** | Registers the exe in `HKCU\...\CurrentVersion\Run` — starts automatically on every login |
| **Run once** | Tracks only the current session without registering |
| **Cancel** | Exits |

> You can also manage registration at any time from the command line (see [Auto-start](#auto-start)).

### 2. Where should data be stored?

| Choice | Effect |
|--------|--------|
| **Sync to GitHub Gist** | Opens a PAT input window; saves token and creates a private Gist |
| **Save locally only** | No token needed; writes `sessions.json` next to the exe |
| **Cancel** | Exits without saving |

For the **GitHub Gist** option you will need a Personal Access Token:

1. Go to **[github.com/settings/tokens/new](https://github.com/settings/tokens/new?scopes=gist)**
2. Select scope: **`gist`** only
3. Copy the token and paste it into the input window

Both choices are saved to `config.ini` automatically and will not be asked again.

---

## Setup

### Customise `config.ini`

`config.ini` is created and populated automatically on first run. You can edit it at any time:

```ini
[github]
token=ghp_your_token_here   ; set by setup dialog — only present in gist mode
gist_id=                    ; filled automatically on first Gist run

[settings]
sync_mode=gist              ; gist | local  (set by setup dialog)
poll_interval_seconds=10    ; how often to scan running processes
sync_interval_minutes=5     ; how often to push to GitHub Gist (or write sessions.json)

[games]
cs2.exe=Counter-Strike 2
ScooterFlow.exe=ScooterFlow
; add more games:
; minecraft.exe=Minecraft
; roblox.exe=Roblox
```

### Run

```bat
build\ChildUsageTracker.exe
```

The process starts invisibly. A tray balloon confirms startup (or shows what is misconfigured).

---

## Auto-start

Manage the Windows login auto-start entry manually if needed:

```bat
build\ChildUsageTracker.exe /install    # register in HKCU\...\Run
build\ChildUsageTracker.exe /uninstall  # remove from HKCU\...\Run
```

---

## Dashboard

1. Open `dashboard/index.html` in any browser
2. Load your data:
   - **Gist mode:** paste the Gist URL shown in the startup notification (e.g. `https://gist.github.com/yourname/abc123`) and click **Load Gist**
   - **Local mode:** click **Load local file** and pick the `sessions.json` file from next to the exe
3. Use the timeframe buttons to narrow the view

> **Private Gist note:** The browser fetches Gist data via the public GitHub API. Private Gists require authentication. Either make the Gist public in GitHub settings, or open the dashboard while logged into GitHub in the same browser (GitHub’s API returns private Gist data for authenticated browser sessions via cookies).

---

## Project structure

```
random-app/
├── build.bat                  # One-click build script (VS 2019 + Ninja)
├── CMakeLists.txt
├── config.ini                 # Template config — populated automatically on first run
├── dashboard/
│   └── index.html             # Parent dashboard (standalone, no server needed)
└── src/
    ├── main.cpp               # WinMain, worker thread, mutex, tray notifications
    ├── dialogs.h / .cpp       # Install prompt + first-run setup + PAT input window
    ├── config.h / .cpp        # INI file reader/writer
    ├── process_monitor.h/.cpp # TlHelp32 process snapshot
    ├── tracker.h / .cpp       # Session model, time accumulation, JSON
    └── gist_client.h / .cpp   # WinHTTP → api.github.com (create / update / fetch)
```

---

## Gist data format

The tracker writes a single file `child_usage_log.json` to a private Gist:

```jsonc
{
  "version": "1.0",
  "last_updated": "2026-04-17T18:32:00Z",
  "sessions": [
    {
      "game": "cs2.exe",
      "name": "Counter-Strike 2",
      "date": "2026-04-17",
      "start": "2026-04-17T15:00:00Z",
      "end":   "2026-04-17T16:23:00Z",
      "duration_seconds": 4980
    }
  ],
  "active_sessions": {
    "scooterflow.exe": {
      "name": "ScooterFlow",
      "start": "2026-04-17T18:10:00Z",
      "running_seconds": 1320
    }
  },
  "daily_totals": {
    "2026-04-17": {
      "Counter-Strike 2": 4980,
      "ScooterFlow": 1320
    }
  }
}
```

All timestamps are **UTC ISO-8601**. The dashboard converts them to the viewer's local timezone.

---

## Security notes

- The GitHub PAT is stored in plaintext in `config.ini` — keep this file protected (readable only by the admin account, not the child’s account)
- The Gist is created as **private** — it is not publicly visible
- In **local mode** no token is stored and no network calls are made
- `gist_token.txt` and the `build/` folder are excluded from git via `.gitignore`
- The PAT only needs the `gist` scope — it cannot access repositories or other GitHub resources

---

## Tech stack

| Component | Technology |
|-----------|------------|
| Tracker | C++17 · MSVC 14.29 · Win32 API |
| Setup dialogs | `TaskDialogIndirect` + custom Win32 window (comctl32) |
| Process detection | `TlHelp32` (CreateToolhelp32Snapshot) |
| HTTP / HTTPS | `WinHTTP` (built-in Windows) |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (auto-downloaded) |
| Build | CMake 3.16+ · Ninja |
| Dashboard | Vanilla HTML/CSS/JS · [Chart.js](https://www.chartjs.org/) v4.4.4 |

---

## License

MIT
