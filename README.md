# ChildUsageTracker

A lightweight Windows background process that silently tracks how long your child plays specific games, syncs a rolling session log to a **private GitHub Gist**, and comes with a **parent-only web dashboard** for reviewing stats, charts, and trends.

---

## Features

### Tracker (`ChildUsageTracker.exe`)
- **Completely invisible** — no window, no console, no taskbar entry (Win32 `SUBSYSTEM:WINDOWS`)
- **Single-instance guard** — named mutex prevents duplicate processes
- **Configurable game list** — any `.exe` name, no recompile needed
- **Process polling** — checks running processes every N seconds (default: 10 s)
- **GitHub Gist sync** — pushes a private rolling JSON log every N minutes (default: 5 min)
- **Local backup** — writes `sessions.json` next to the exe on every sync
- **Tray balloon notifications** — startup confirmation, config errors, Gist URL on first run
- **Auto-start** — `/install` and `/uninstall` flags for `HKCU\...\Run` registry entry
- **Clean shutdown sync** — final Gist push on system shutdown / logoff

### Dashboard (`dashboard/index.html`)
- Open directly in any browser — no server, no build step
- Load data by pasting your Gist URL or ID
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
| Windows 10/11 | Runtime target |
| [VS 2019 Build Tools](https://visualstudio.microsoft.com/vs/older-downloads/) | With the **C++ build tools** workload (`visualstudio2019-workload-vctools`) |
| CMake ≥ 3.16 | Bundled with VS Build Tools at `Common7\IDE\CommonExtensions\Microsoft\CMake` |
| Internet access | For GitHub Gist sync |

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

## Setup

### 1. Create a GitHub Personal Access Token

Go to **[github.com/settings/tokens/new](https://github.com/settings/tokens/new?scopes=gist)**  
Select scope: **`gist`** only. Copy the token.

### 2. Configure

Edit `config.ini` (copy it next to `ChildUsageTracker.exe` if running from the build folder):

```ini
[github]
token=ghp_your_token_here   ; ← paste your PAT here
gist_id=                    ; ← leave blank, filled automatically on first run

[settings]
poll_interval_seconds=10    ; how often to scan running processes
sync_interval_minutes=5     ; how often to push to GitHub Gist

[games]
cs2.exe=Counter-Strike 2
ScooterFlow.exe=ScooterFlow
; add more games:
; minecraft.exe=Minecraft
; roblox.exe=Roblox
```

### 3. First run

```bat
build\ChildUsageTracker.exe
```

A tray balloon will appear in the bottom-right corner confirming startup. On first run it creates the private Gist and saves the ID back to `config.ini` automatically.

### 4. Auto-start on login (recommended)

Run once as the child's user account:

```bat
build\ChildUsageTracker.exe /install
```

To remove:

```bat
build\ChildUsageTracker.exe /uninstall
```

---

## Dashboard

1. Open `dashboard/index.html` in any browser
2. Paste the Gist URL shown in the startup notification (e.g. `https://gist.github.com/yourname/abc123`)
3. Click **Load**

> **Private Gist note:** The browser fetches Gist data via the public GitHub API. Private Gists are not accessible without authentication. Either make the Gist public in your GitHub account settings, or open the dashboard while logged into GitHub in the same browser session (GitHub's API returns private Gist data for authenticated users via browser cookies).

---

## Project structure

```
random-app/
├── build.bat                  # One-click build script (VS 2019 + Ninja)
├── CMakeLists.txt
├── config.ini                 # Template config — copy next to .exe and set token=
├── dashboard/
│   └── index.html             # Parent dashboard (standalone, no server needed)
└── src/
    ├── main.cpp               # WinMain, worker thread, mutex, tray notifications
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

- The GitHub PAT is stored in plaintext in `config.ini` — keep this file protected (readable only by the admin account, not the child's account)
- The Gist is created as **private** — it is not publicly visible
- `gist_token.txt` and the `build/` folder are excluded from git via `.gitignore`
- The PAT only needs the `gist` scope — it cannot access repositories or other GitHub resources

---

## Tech stack

| Component | Technology |
|-----------|-----------|
| Tracker | C++17 · MSVC 14.29 · Win32 API |
| Process detection | `TlHelp32` (CreateToolhelp32Snapshot) |
| HTTP / HTTPS | `WinHTTP` (built-in Windows) |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (auto-downloaded) |
| Build | CMake 3.16+ · Ninja |
| Dashboard | Vanilla HTML/CSS/JS · [Chart.js](https://www.chartjs.org/) v4.4.4 |

---

## License

MIT
