#pragma once
#include "core/Project.h"
#include <filesystem>
#include <nlohmann/json.hpp>
namespace rb {
nlohmann::json projectJson(const Project &);
Project projectFromJson(const nlohmann::json &);
Project projectFromAnimationJson(const nlohmann::json &);
Project loadProject(const std::filesystem::path &);
void saveProject(const Project &, const std::filesystem::path &);
nlohmann::json animationJson(const Project &, std::size_t clip, int fps = 60);
void exportAnimation(const Project &, std::size_t clip, const std::filesystem::path &, int fps = 60);
} // namespace rb
