#pragma once
#include "core/Project.h"
#include <filesystem>
namespace rb {
std::size_t importModel(Project &, const std::filesystem::path &, float scale = 1);
}
