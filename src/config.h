#pragma once
#include <map>
#include <string>

// Simple INI-file configuration reader/writer.
// Sections and keys are stored in std::map (alphabetical order on save).
// Comments in the original file are not preserved after save().
class Config {
public:
    // Load from file; returns false if file cannot be opened (data stays empty).
    bool load(const std::wstring& path);

    // Write current data back to the same path; returns false on failure.
    bool save() const;

    // Read a value; returns defaultValue when section/key does not exist.
    std::string get(const std::string& section,
                    const std::string& key,
                    const std::string& defaultValue = "") const;

    // Write (or overwrite) a value in memory. Call save() to persist.
    void set(const std::string& section,
             const std::string& key,
             const std::string& value);

    // Return all key-value pairs in a section (empty map if section missing).
    std::map<std::string, std::string> getSection(const std::string& section) const;

private:
    std::wstring path_;
    std::map<std::string, std::map<std::string, std::string>> data_;
};
