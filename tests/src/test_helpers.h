#pragma once

#include "packer.h"

#include "stb_image_write.h"

#include <moth_ui/layout/layout.h>
#include <moth_ui/layout/layout_entity_image.h>
#include <moth_ui/layout/layout_rect.h>

#ifdef _WIN32
#include <process.h>
#define MOTH_PACKER_TESTS_PID() _getpid()
#else
#include <unistd.h>
#define MOTH_PACKER_TESTS_PID() getpid()
#endif

#include <atomic>
#include <filesystem>

// RAII temporary directory — created on construction, recursively deleted on destruction
struct TempDir {
    std::filesystem::path path;

    TempDir() {
        static std::atomic<int> s_counter{ 0 };
        // Include PID so parallel CTest processes (each starting counter at 0) don't collide
        auto const name = "moth_packer_tests_" + std::to_string(MOTH_PACKER_TESTS_PID()) + "_" + std::to_string(++s_counter);
        path = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directories(path);
    }

    ~TempDir() {
        std::filesystem::remove_all(path);
    }

    TempDir(TempDir const&) = delete;
    TempDir& operator=(TempDir const&) = delete;
};

// Write a solid-colour RGBA PNG of the given dimensions and return an ImageDetails for it
inline ImageDetails MakeTestImage(std::filesystem::path const& dir, std::string const& name, int width, int height) {
    auto const filePath = dir / name;
    int const channels = 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * channels, 255);
    stbi_write_png(filePath.string().c_str(), width, height, channels, pixels.data(), width * channels);

    ImageDetails details;
    details.path = filePath;
    details.dimensions = { width, height };
    details.channels = channels;
    return details;
}

// Create a layout file containing image entities pointing to the given absolute image paths.
// Image paths must be absolute so that GetLoadedPath() / m_imagePath resolves correctly.
inline std::filesystem::path MakeTestLayout(std::filesystem::path const& dir, std::string const& name, std::vector<std::filesystem::path> const& imagePaths) {
    moth_ui::Layout layout;
    auto const defaultRect = moth_ui::MakeDefaultLayoutRect();
    for (auto const& imagePath : imagePaths) {
        auto entity = std::make_shared<moth_ui::LayoutEntityImage>(defaultRect, imagePath);
        layout.m_children.push_back(entity);
    }
    auto const layoutPath = dir / (name + moth_ui::Layout::FullExtension);
    layout.Save(layoutPath);
    return layoutPath;
}
