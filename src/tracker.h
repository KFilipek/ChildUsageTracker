#pragma once
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct Session {
    std::string game_key;         // lowercased exe name
    std::string display_name;     // human-readable name from config
    std::string date;             // YYYY-MM-DD (UTC, derived from start time)
    std::string start_time;       // ISO-8601 UTC
    std::string end_time;         // ISO-8601 UTC
    long long   duration_seconds = 0;
};

class Tracker {
public:
    // Call once after loading config — provides the exe→name mapping.
    void setGames(const std::map<std::string, std::string>& games);

    // Feed the current set of running process names (lowercased basenames).
    // Detects start/stop events and updates session state accordingly.
    void update(const std::set<std::wstring>& running_processes);

    // Serialise full history (completed + in-progress) to JSON.
    nlohmann::json toJson() const;

    // Restore completed session history from a previously serialised JSON blob.
    // Active sessions are intentionally not restored (they would be stale).
    void loadFromJson(const nlohmann::json& j);

    bool isDirty() const                 { return dirty_; }
    void clearDirty()                  { dirty_ = false; }
    size_t completedSessionCount() const { return completed_sessions_.size(); }

private:
    std::string FormatTime(const std::chrono::system_clock::time_point& tp) const;
    std::string FormatDate(const std::chrono::system_clock::time_point& tp) const;

    // exe_lower → display name
    std::map<std::string, std::string> games_;

    std::vector<Session> completed_sessions_;

    // exe_lower → session start time (games currently running)
    std::map<std::string, std::chrono::system_clock::time_point> active_sessions_;

    bool dirty_ = false;
};
