#pragma once
#include <string>

// WinHTTP-based GitHub Gist client.
// All methods are synchronous and blocking.
// Returns empty string / false on network or API error.
class GistClient {
public:
    explicit GistClient(const std::string& token);

    // POST /gists — creates a new private gist, returns the gist ID.
    std::string create(const std::string& description,
                       const std::string& filename,
                       const std::string& content);

    // PATCH /gists/{id} — updates one file inside an existing gist.
    bool update(const std::string& gist_id,
                const std::string& filename,
                const std::string& content);

    // GET /gists/{id} — fetches the content of one file from a gist.
    std::string fetch(const std::string& gist_id,
                      const std::string& filename);

private:
    // Raw HTTP helper. Returns full response body or empty string on error.
    std::string MakeRequest(const std::wstring& method,
                            const std::wstring& path,
                            const std::string&  body);

    std::string token_;
};
