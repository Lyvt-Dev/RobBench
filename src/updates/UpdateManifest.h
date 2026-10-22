#pragma once
#include <array>
#include <nlohmann/json.hpp>
#include <string>
namespace rb {
using AppVersion = std::array<unsigned, 3>;
AppVersion parseVersion(std::string);
std::string checksumFor(const std::string &manifest, const std::string &filename);
struct UpdateInfo {
    std::string version, notes, filename, browserUrl, apiUrl, sumsBrowserUrl, sumsApiUrl;
    std::size_t size{};
};
UpdateInfo parseUpdate(const nlohmann::json &);
} // namespace rb
