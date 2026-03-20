#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION

#include "packer.h"

#include <moth_ui/moth_ui.h>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/remove_if.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_rect_pack.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace {
    struct ImageDetails {
        std::filesystem::path path;
        moth_ui::IntVec2 dimensions;
        int channels = 0;
    };

    // recursively loads all layouts in a given path
    void CollectLayouts(std::filesystem::path const& path, std::vector<std::shared_ptr<moth_ui::Layout>>& layouts) {
        for (auto&& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry.path())) {
                CollectLayouts(entry.path(), layouts);
            } else if (entry.path().has_extension() && entry.path().extension() == moth_ui::Layout::FullExtension) {
                std::shared_ptr<moth_ui::Layout> layout;
                auto const result = moth_ui::Layout::Load(entry.path(), &layout);
                if (result == moth_ui::Layout::LoadResult::Success) {
                    layouts.push_back(layout);
                }
            }
        }
    }

    void CollectImages(moth_ui::Layout const& layout, std::vector<ImageDetails>& images) {
        for (auto&& childEntity : layout.m_children) {
            if (childEntity->GetType() == moth_ui::LayoutEntityType::Image) {
                auto& imageEntity = dynamic_cast<moth_ui::LayoutEntityImage&>(*childEntity);
                auto imagePath = layout.GetLoadedPath() / imageEntity.m_imagePath;
                // dont add duplicates
                if (std::end(images) == ranges::find_if(images, [&](auto const& detail) { return detail.path == imagePath; })) {
                    ImageDetails details;
                    details.path = imagePath;
                    auto const result = stbi_info(imagePath.string().c_str(), &details.dimensions.x, &details.dimensions.y, &details.channels);
                    if (result != 1) {
                        spdlog::warn("Failed to read image info: {}", imagePath.string());
                        continue;
                    }
                    images.push_back(details);
                }
            }
        }
    }

    int NextPowerOf2(int value) {
        value--;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value++;
        return value;
    }

    moth_ui::IntVec2 FindOptimalDimensions(std::vector<stbrp_node>& nodes, std::vector<stbrp_rect>& rects, moth_ui::IntVec2 const& minPack, moth_ui::IntVec2 const& maxPack) {
        int totalArea = 0;
        for (auto&& rect : rects) {
            totalArea += rect.w * rect.h;
        }

        struct PackTest {
            moth_ui::IntVec2 m_dimensions;
            float m_ratio = 0.0f;
        };

        int const minDimX = NextPowerOf2(minPack.x);
        int const minDimY = NextPowerOf2(minPack.y);
        int const maxDimX = NextPowerOf2(maxPack.x);
        int const maxDimY = NextPowerOf2(maxPack.y);

        std::vector<PackTest> testDimensions;
        int curWidth = minDimX;
        while (curWidth <= maxDimX) {
            int curHeight = minDimY;
            while (curHeight <= maxDimY) {
                int const curArea = curWidth * curHeight;
                if (curArea > totalArea) {
                    PackTest info;
                    info.m_dimensions = moth_ui::IntVec2{ curWidth, curHeight };
                    testDimensions.push_back(info);
                }
                curHeight *= 2;
            }
            curWidth *= 2;
        }

        for (auto&& testDim : testDimensions) {
            stbrp_context stbContext;
            stbrp_init_target(&stbContext, testDim.m_dimensions.x, testDim.m_dimensions.y, nodes.data(), static_cast<int>(nodes.size()));
            auto const allPacked = stbrp_pack_rects(&stbContext, rects.data(), static_cast<int>(rects.size()));
            if (allPacked != 0) {
                float const testArea = static_cast<float>(testDim.m_dimensions.x * testDim.m_dimensions.y);
                testDim.m_ratio = static_cast<float>(totalArea) / testArea;
            }
        }

        std::sort(std::begin(testDimensions), std::end(testDimensions), [](auto const& a, auto const& b) { return b.m_ratio < a.m_ratio; });
        return std::begin(testDimensions)->m_dimensions;
    }

    void CommitPack(int num, std::filesystem::path const& outputPath, std::string const& filename, int width, int height, std::vector<stbrp_rect>& rects, std::vector<ImageDetails> const& images) {
        int const atlasChannels = 4;
        std::vector<uint8_t> atlasPixels(static_cast<size_t>(width) * height * atlasChannels, 0);

        nlohmann::json packDetails;
        for (auto&& rect : rects) {
            if (rect.was_packed == 0) {
                continue;
            }

            auto const& imagePath = images[rect.id].path;
            int srcWidth = 0;
            int srcHeight = 0;
            int srcChannels = 0;
            stbi_uc* srcPixels = stbi_load(imagePath.string().c_str(), &srcWidth, &srcHeight, &srcChannels, atlasChannels);
            if (srcPixels == nullptr) {
                spdlog::warn("Failed to load image for packing: {}", imagePath.string());
                continue;
            }

            for (int row = 0; row < rect.h; ++row) {
                auto const srcOffset = static_cast<size_t>(row) * rect.w * atlasChannels;
                auto const dstOffset = (static_cast<size_t>(rect.y + row) * width + rect.x) * atlasChannels;
                std::memcpy(atlasPixels.data() + dstOffset, srcPixels + srcOffset, static_cast<size_t>(rect.w) * atlasChannels);
            }

            stbi_image_free(srcPixels);

            nlohmann::json details;
            auto const relativePath = std::filesystem::relative(imagePath, outputPath);
            details["path"] = relativePath.string();
            details["rect"] = { { "x", rect.x }, { "y", rect.y }, { "w", rect.w }, { "h", rect.h } };
            packDetails.push_back(details);
        }

        // save atlas PNG
        auto const imagePackName = fmt::format("{}_{}.png", filename, num);
        auto const imagePngPath = outputPath / imagePackName;
        if (stbi_write_png(imagePngPath.string().c_str(), width, height, atlasChannels, atlasPixels.data(), width * atlasChannels) == 0) {
            spdlog::error("Failed to write atlas PNG: {}", imagePngPath.string());
        }

        // save descriptor JSON
        auto const packDetailsName = fmt::format("{}_{}.json", filename, num);
        std::ofstream ofile(outputPath / packDetailsName);
        if (ofile.is_open()) {
            nlohmann::json detailsRoot;
            detailsRoot["images"] = packDetails;
            ofile << detailsRoot;
        }

        // remove packed rects
        rects.erase(ranges::remove_if(rects, [](auto const& r) { return r.was_packed != 0; }), std::end(rects));
    }
}

void Pack(std::filesystem::path const& inputPath, std::filesystem::path const& outputPath, std::string const& filename, int minWidth, int minHeight, int maxWidth, int maxHeight) {
    if (!std::filesystem::exists(inputPath)) {
        spdlog::error("Input path does not exist: {}", inputPath.string());
        return;
    }

    if (!std::filesystem::exists(outputPath)) {
        spdlog::error("Output path does not exist: {}", outputPath.string());
        return;
    }

    std::vector<std::shared_ptr<moth_ui::Layout>> layouts;
    CollectLayouts(inputPath, layouts);

    std::vector<ImageDetails> images;
    for (auto&& layout : layouts) {
        CollectImages(*layout, images);
    }

    if (images.empty()) {
        spdlog::info("No images found in {}", inputPath.string());
        return;
    }

    std::vector<stbrp_rect> stbRects;
    stbRects.reserve(images.size());
    int i = 0;
    for (auto&& image : images) {
        stbrp_rect r;
        r.id = i;
        r.w = image.dimensions.x;
        r.h = image.dimensions.y;
        if (r.w <= maxWidth && r.h <= maxHeight) {
            stbRects.push_back(r);
        } else {
            spdlog::warn("Image {} exceeds max atlas size, skipping", image.path.string());
        }
        ++i;
    }

    std::vector<stbrp_node> stbNodes(maxWidth * 2);

    int numPacks = 0;
    while (!stbRects.empty()) {
        auto const packDim = FindOptimalDimensions(stbNodes, stbRects, { minWidth, minHeight }, { maxWidth, maxHeight });

        stbrp_context stbContext;
        stbrp_init_target(&stbContext, packDim.x, packDim.y, stbNodes.data(), static_cast<int>(stbNodes.size()));
        stbrp_pack_rects(&stbContext, stbRects.data(), static_cast<int>(stbRects.size()));
        CommitPack(numPacks, outputPath, filename, packDim.x, packDim.y, stbRects, images);
        ++numPacks;
    }

    spdlog::info("Packed {} images into {} atlas(es)", images.size(), numPacks);
}
