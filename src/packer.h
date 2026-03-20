#pragma once

#include <filesystem>

void Pack(std::filesystem::path const& inputPath, std::filesystem::path const& outputPath, std::string const& filename, int minWidth, int minHeight, int maxWidth, int maxHeight);
