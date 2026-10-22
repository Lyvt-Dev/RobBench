#include "updates/UpdateManifest.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <stdexcept>
namespace rb {
AppVersion parseVersion(std::string value) {
    if (!value.empty() && value.front() == 'v')
        value.erase(0, 1);
    AppVersion version{};
    std::size_t begin = 0;
    for (int i = 0; i < 3; ++i) {
        auto end = value.find('.', begin);
        if (i == 2)
            end = value.size();
        if (end == std::string::npos || end == begin)
            throw std::runtime_error("Invalid release version");
        auto result = std::from_chars(value.data() + begin, value.data() + end, version[i]);
        if (result.ec != std::errc() || result.ptr != value.data() + end || version[i] > 1000000)
            throw std::runtime_error("Invalid release version");
        begin = end + 1;
    }
    return version;
}
std::string checksumFor(const std::string &manifest, const std::string &filename) {
    std::istringstream input(manifest);
    std::string line, result;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string hash, name;
        row >> hash >> name;
        if (!name.empty() && name.front() == '*')
            name.erase(0, 1);
        if (name != filename)
            continue;
        if (hash.size() != 64 ||
            !std::all_of(hash.begin(), hash.end(), [](unsigned char c) { return std::isxdigit(c); }))
            throw std::runtime_error("Invalid SHA256 checksum");
        std::transform(hash.begin(), hash.end(), hash.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!result.empty())
            throw std::runtime_error("Duplicate checksum entries");
        result = hash;
    }
    if (result.empty())
        throw std::runtime_error("Installer is missing from SHA256SUMS");
    return result;
}
UpdateInfo parseUpdate(const nlohmann::json &json) {
    if (json.value("draft", false) || json.value("prerelease", false))
        throw std::runtime_error("Not a stable release");
    UpdateInfo result;
    result.version = json.at("tag_name").get<std::string>();
    parseVersion(result.version);
    result.notes = json.value("body", std::string());
    auto version = result.version;
    if (version.front() == 'v')
        version.erase(0, 1);
    result.filename = "RobBench-" + version + "-win64.exe";
    for (const auto &asset : json.at("assets")) {
        auto name = asset.at("name").get<std::string>();
        if (name == result.filename) {
            result.browserUrl = asset.at("browser_download_url");
            result.apiUrl = asset.at("url");
            result.size = asset.at("size");
        } else if (name == "SHA256SUMS") {
            result.sumsBrowserUrl = asset.at("browser_download_url");
            result.sumsApiUrl = asset.at("url");
        }
    }
    if (result.browserUrl.empty() || result.apiUrl.empty() || result.sumsBrowserUrl.empty() ||
        result.sumsApiUrl.empty() || result.size == 0 || result.size > 512 * 1024 * 1024)
        throw std::runtime_error("Release has no supported Windows installer and SHA256SUMS pair");
    return result;
}
} // namespace rb
