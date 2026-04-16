#include "config.h"

#include <fstream>
#include <sstream>

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

// ── Config ────────────────────────────────────────────────────────────────────

bool Config::load(const std::wstring& path) {
    path_ = path;
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string current_section;
    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
        } else {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key   = Trim(line.substr(0, eq));
                std::string value = Trim(line.substr(eq + 1));
                data_[current_section][key] = value;
            }
        }
    }
    return true;
}

bool Config::save() const {
    std::ofstream file(path_);
    if (!file.is_open()) return false;

    for (const auto& [section, kv] : data_) {
        file << "[" << section << "]\n";
        for (const auto& [key, value] : kv) {
            file << key << "=" << value << "\n";
        }
        file << "\n";
    }
    return true;
}

std::string Config::get(const std::string& section,
                        const std::string& key,
                        const std::string& defaultValue) const {
    auto sit = data_.find(section);
    if (sit == data_.end()) return defaultValue;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return defaultValue;
    return kit->second;
}

void Config::set(const std::string& section,
                 const std::string& key,
                 const std::string& value) {
    data_[section][key] = value;
}

std::map<std::string, std::string> Config::getSection(const std::string& section) const {
    auto it = data_.find(section);
    if (it == data_.end()) return {};
    return it->second;
}
