#include "tracker.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

// ── helpers ───────────────────────────────────────────────────────────────────

std::string Tracker::FormatTime(const std::chrono::system_clock::time_point& tp) const {
    time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm utc{};
    gmtime_s(&utc, &t);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string Tracker::FormatDate(const std::chrono::system_clock::time_point& tp) const {
    time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm utc{};
    gmtime_s(&utc, &t);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%d");
    return oss.str();
}

// ── Tracker ───────────────────────────────────────────────────────────────────

void Tracker::setGames(const std::map<std::string, std::string>& games) {
    games_.clear();
    for (const auto& [exe, name] : games) {
        std::string lower = exe;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        games_[lower] = name;
    }
}

void Tracker::update(const std::set<std::wstring>& running_processes) {
    const auto now = std::chrono::system_clock::now();

    for (const auto& [exe_lower, display_name] : games_) {
        // Build the wstring key from the ASCII exe name (safe for ASCII-only exe names).
        std::wstring exe_wide(exe_lower.begin(), exe_lower.end());

        const bool is_running  = running_processes.count(exe_wide) > 0;
        const bool was_active  = active_sessions_.count(exe_lower) > 0;

        if (is_running && !was_active) {
            // ── game started ──────────────────────────────────────────────────
            active_sessions_[exe_lower] = now;
            dirty_ = true;

        } else if (!is_running && was_active) {
            // ── game stopped — close the session ─────────────────────────────
            const auto start    = active_sessions_.at(exe_lower);
            const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

            Session s;
            s.game_key        = exe_lower;
            s.display_name    = display_name;
            s.date            = FormatDate(start);
            s.start_time      = FormatTime(start);
            s.end_time        = FormatTime(now);
            s.duration_seconds = duration;

            completed_sessions_.push_back(std::move(s));
            active_sessions_.erase(exe_lower);
            dirty_ = true;
        }
    }
}

nlohmann::json Tracker::toJson() const {
    const auto now = std::chrono::system_clock::now();

    nlohmann::json j;
    j["version"]      = "1.0";
    j["last_updated"] = FormatTime(now);

    // ── completed sessions ────────────────────────────────────────────────────
    j["sessions"] = nlohmann::json::array();
    for (const auto& s : completed_sessions_) {
        j["sessions"].push_back({
            {"game",             s.game_key},
            {"name",             s.display_name},
            {"date",             s.date},
            {"start",            s.start_time},
            {"end",              s.end_time},
            {"duration_seconds", s.duration_seconds}
        });
    }

    // ── in-progress sessions ──────────────────────────────────────────────────
    j["active_sessions"] = nlohmann::json::object();
    for (const auto& [exe, start] : active_sessions_) {
        const auto running_secs =
            std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        const auto it = games_.find(exe);
        const std::string display = (it != games_.end()) ? it->second : exe;

        j["active_sessions"][exe] = {
            {"name",            display},
            {"start",           FormatTime(start)},
            {"running_seconds", running_secs}
        };
    }

    // ── daily totals (completed + running partial) ────────────────────────────
    std::map<std::string, std::map<std::string, long long>> daily;

    for (const auto& s : completed_sessions_)
        daily[s.date][s.display_name] += s.duration_seconds;

    for (const auto& [exe, start] : active_sessions_) {
        const auto running_secs =
            std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        const auto it = games_.find(exe);
        const std::string display = (it != games_.end()) ? it->second : exe;
        daily[FormatDate(start)][display] += running_secs;
    }

    j["daily_totals"] = nlohmann::json::object();
    for (const auto& [date, game_map] : daily) {
        j["daily_totals"][date] = nlohmann::json::object();
        for (const auto& [game, total] : game_map)
            j["daily_totals"][date][game] = total;
    }

    return j;
}

void Tracker::loadFromJson(const nlohmann::json& j) {
    completed_sessions_.clear();
    active_sessions_.clear();

    if (j.contains("sessions") && j["sessions"].is_array()) {
        for (const auto& item : j["sessions"]) {
            Session s;
            s.game_key         = item.value("game",             std::string{});
            s.display_name     = item.value("name",             std::string{});
            s.date             = item.value("date",             std::string{});
            s.start_time       = item.value("start",            std::string{});
            s.end_time         = item.value("end",              std::string{});
            s.duration_seconds = item.value("duration_seconds", 0LL);
            if (!s.game_key.empty())
                completed_sessions_.push_back(std::move(s));
        }
    }

    dirty_ = false;
}
