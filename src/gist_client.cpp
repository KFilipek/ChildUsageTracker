#include "gist_client.h"

#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <vector>

// ── string conversion ─────────────────────────────────────────────────────────

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), size);
    return result;
}

// ── RAII handle wrapper ───────────────────────────────────────────────────────

struct WinHttpHandles {
    HINTERNET session  = nullptr;
    HINTERNET connect  = nullptr;
    HINTERNET request  = nullptr;

    ~WinHttpHandles() {
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
    }
};

// ── GistClient ────────────────────────────────────────────────────────────────

GistClient::GistClient(const std::string& token) : token_(token) {}

std::string GistClient::MakeRequest(const std::wstring& method,
                                     const std::wstring& path,
                                     const std::string&  body) {
    WinHttpHandles h;

    h.session = WinHttpOpen(
        L"ChildUsageTracker/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!h.session) return {};

    h.connect = WinHttpConnect(h.session, L"api.github.com",
                               INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!h.connect) return {};

    h.request = WinHttpOpenRequest(
        h.connect,
        method.c_str(),
        path.c_str(),
        nullptr,                        // HTTP/1.1
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);           // HTTPS
    if (!h.request) return {};

    // ── add headers ───────────────────────────────────────────────────────────
    const std::wstring token_wide = Utf8ToWide(token_);
    const std::wstring headers =
        L"Authorization: Bearer " + token_wide + L"\r\n"
        L"Content-Type: application/json\r\n"
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";

    WinHttpAddRequestHeaders(h.request,
                             headers.c_str(),
                             static_cast<DWORD>(headers.size()),
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    // ── send ──────────────────────────────────────────────────────────────────
    LPVOID bodyData = body.empty() ? nullptr
                                   : static_cast<LPVOID>(const_cast<char*>(body.c_str()));
    const DWORD bodyLen = static_cast<DWORD>(body.size());

    if (!WinHttpSendRequest(h.request,
                            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            bodyData, bodyLen, bodyLen,
                            0 /* context */))
        return {};

    if (!WinHttpReceiveResponse(h.request, nullptr))
        return {};

    // ── read response ─────────────────────────────────────────────────────────
    std::string response;
    DWORD bytesAvailable = 0;
    do {
        if (!WinHttpQueryDataAvailable(h.request, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;

        std::vector<char> buf(bytesAvailable + 1, '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(h.request, buf.data(), bytesAvailable, &bytesRead))
            break;
        response.append(buf.data(), bytesRead);
    } while (bytesAvailable > 0);

    return response;
}

std::string GistClient::create(const std::string& description,
                                const std::string& filename,
                                const std::string& content) {
    nlohmann::json body;
    body["description"]        = description;
    body["public"]             = false;
    body["files"][filename]["content"] = content;

    const std::string response = MakeRequest(L"POST", L"/gists", body.dump());
    if (response.empty()) return {};

    try {
        const auto j = nlohmann::json::parse(response);
        return j.value("id", std::string{});
    } catch (...) {
        return {};
    }
}

bool GistClient::update(const std::string& gist_id,
                         const std::string& filename,
                         const std::string& content) {
    nlohmann::json body;
    body["files"][filename]["content"] = content;

    const std::wstring path = L"/gists/" + Utf8ToWide(gist_id);
    const std::string response = MakeRequest(L"PATCH", path, body.dump());
    return !response.empty();
}

std::string GistClient::fetch(const std::string& gist_id,
                               const std::string& filename) {
    const std::wstring path = L"/gists/" + Utf8ToWide(gist_id);
    const std::string response = MakeRequest(L"GET", path, {});
    if (response.empty()) return {};

    try {
        const auto j = nlohmann::json::parse(response);
        if (!j.contains("files") || !j["files"].contains(filename))
            return {};

        const auto& file = j["files"][filename];
        // If content is truncated (>10 MB, extremely unlikely for a usage log)
        // we fall back to the raw_url field.
        if (file.value("truncated", false)) {
            // raw_url is on a different host; skip gracefully — history will
            // be re-accumulated from this point forward.
            return {};
        }
        return file.value("content", std::string{});
    } catch (...) {
        return {};
    }
}
