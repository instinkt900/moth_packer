#include "moth_packer/packer.h"

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
#include <unordered_set>
#include <vector>

namespace moth_packer {

namespace {
    std::unordered_set<std::string> const kSupportedExtensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };

    // recursively loads all layouts in a given path
    void CollectLayouts(std::filesystem::path const& path, bool recursive, std::vector<std::shared_ptr<moth_ui::Layout>>& layouts) {
        for (auto&& entry : std::filesystem::directory_iterator(path)) {
            if (recursive && std::filesystem::is_directory(entry.path())) {
                CollectLayouts(entry.path(), recursive, layouts);
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

        // total area exceeds max atlas size — use the largest available to pack as many as possible
        if (testDimensions.empty()) {
            return { maxDimX, maxDimY };
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

        // no single dimension fit all rects — use the largest to pack as many as possible
        if (std::begin(testDimensions)->m_ratio == 0.0f) {
            return { maxDimX, maxDimY };
        }

        return std::begin(testDimensions)->m_dimensions;
    }

    // Composites packed rects into a PNG and returns the atlas JSON entry for the descriptor.
    // Erases packed rects from the list regardless of success.
    nlohmann::json CommitPack(std::filesystem::path const& imagePngPath, std::filesystem::path const& outputPath, bool dryRun, int width, int height, std::vector<stbrp_rect>& rects, std::vector<ImageDetails> const& images) {
        int const atlasChannels = 4;
        std::vector<uint8_t> atlasPixels(static_cast<size_t>(width) * height * atlasChannels, 0);

        nlohmann::json atlasImages;
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
                spdlog::error("Failed to load image for packing: {}", imagePath.string());
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
            atlasImages.push_back(details);
            spdlog::info("Packed {} into {}", relativePath.string(), imagePngPath.string());
        }

        if (!dryRun && stbi_write_png(imagePngPath.string().c_str(), width, height, atlasChannels, atlasPixels.data(), width * atlasChannels) == 0) {
            spdlog::error("Failed to write atlas PNG: {}", imagePngPath.string());
            return {};
        }

        rects.erase(ranges::remove_if(rects, [](auto const& r) { return r.was_packed != 0; }), std::end(rects));

        nlohmann::json atlasEntry;
        atlasEntry["atlas"] = std::filesystem::relative(imagePngPath, outputPath).string();
        atlasEntry["images"] = atlasImages;
        return atlasEntry;
    }
}

bool CollectImagesFromFile(std::filesystem::path const& inputList, std::vector<ImageDetails>& dstList) {
    if (!std::filesystem::exists(inputList)) {
        spdlog::error("Input path does not exist: {}", inputList.string());
        return false;
    }

    std::vector<ImageDetails> images;

    std::ifstream file(inputList);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        std::filesystem::path imagePath(line);
        auto const ext = imagePath.extension().string();
        if (kSupportedExtensions.find(ext) == std::end(kSupportedExtensions)) {
            spdlog::warn("Unsupported image format, skipping: {}", line);
            continue;
        }
        if (!std::filesystem::exists(imagePath)) {
            spdlog::warn("Image not found, skipping: {}", line);
            continue;
        }
        // skip duplicates
        auto const isDuplicate = [&](ImageDetails const& d) { return d.path == imagePath; };
        if (ranges::find_if(dstList, isDuplicate) != std::end(dstList) ||
            ranges::find_if(images, isDuplicate) != std::end(images)) {
            continue;
        }
        ImageDetails details;
        details.path = imagePath;
        if (stbi_info(imagePath.string().c_str(), &details.dimensions.x, &details.dimensions.y, &details.channels) != 1) {
            spdlog::warn("Failed to read image info: {}", line);
            continue;
        }
        images.push_back(details);
    }

    if (images.empty()) {
        spdlog::info("No images found in {}", inputList.string());
        return false;
    }

    dstList.insert(std::end(dstList), std::begin(images), std::end(images));
    return true;
}

bool CollectImagesFromDir(std::filesystem::path const& inputPath, bool const& recursive, std::vector<ImageDetails>& dstList) {
    if (!std::filesystem::exists(inputPath)) {
        spdlog::error("Input path does not exist: {}", inputPath.string());
        return false;
    }

    std::vector<ImageDetails> images;

    auto collectFromIterator = [&](auto&& iterator) {
        for (auto&& entry : iterator) {
            if (!entry.is_regular_file()) {
                continue;
            }
            auto const ext = entry.path().extension().string();
            if (kSupportedExtensions.find(ext) == std::end(kSupportedExtensions)) {
                continue;
            }
            auto const& imagePath = entry.path();
            // skip duplicates
            auto const isDuplicate = [&](ImageDetails const& d) { return d.path == imagePath; };
            if (ranges::find_if(dstList, isDuplicate) != std::end(dstList) ||
                ranges::find_if(images, isDuplicate) != std::end(images)) {
                continue;
            }
            ImageDetails details;
            details.path = imagePath;
            if (stbi_info(imagePath.string().c_str(), &details.dimensions.x, &details.dimensions.y, &details.channels) != 1) {
                spdlog::warn("Failed to read image info: {}", imagePath.string());
                continue;
            }
            images.push_back(details);
        }
    };

    if (recursive) {
        collectFromIterator(std::filesystem::recursive_directory_iterator(inputPath));
    } else {
        collectFromIterator(std::filesystem::directory_iterator(inputPath));
    }

    if (images.empty()) {
        spdlog::info("No images found in {}", inputPath.string());
        return false;
    }

    dstList.insert(std::end(dstList), std::begin(images), std::end(images));
    return true;
}

bool CollectImagesFromLayout(std::filesystem::path const& inputLayout, std::vector<ImageDetails>& dstList) {
    if (!std::filesystem::exists(inputLayout)) {
        spdlog::error("Input layout does not exist: {}", inputLayout.string());
        return false;
    }

    std::shared_ptr<moth_ui::Layout> layout;
    auto const result = moth_ui::Layout::Load(inputLayout, &layout);
    if (result != moth_ui::Layout::LoadResult::Success) {
        spdlog::error("Failed to open layout: {}", inputLayout.string());
        return false;
    }

    std::vector<ImageDetails> images;
    CollectImages(*layout, images);

    if (images.empty()) {
        spdlog::info("No images found in {}", inputLayout.string());
        return false;
    }

    dstList.insert(std::end(dstList), std::begin(images), std::end(images));
    return true;
}

bool CollectImagesFromLayoutsDir(std::filesystem::path const& inputPath, bool recursive, std::vector<ImageDetails>& dstList) {
    if (!std::filesystem::exists(inputPath)) {
        spdlog::error("Input path does not exist: {}", inputPath.string());
        return false;
    }

    std::vector<std::shared_ptr<moth_ui::Layout>> layouts;
    CollectLayouts(inputPath, recursive, layouts);

    std::vector<ImageDetails> images;
    for (auto&& layout : layouts) {
        CollectImages(*layout, images);
    }

    if (images.empty()) {
        spdlog::info("No images found in {}", inputPath.string());
        return false;
    }

    dstList.insert(std::end(dstList), std::begin(images), std::end(images));
    return true;
}

bool Pack(std::vector<ImageDetails> images, std::filesystem::path const& outputPath, std::string const& filename, bool forceOverwrite, bool dryRun, int minWidth, int minHeight, int maxWidth, int maxHeight) {
    if (images.empty()) {
        spdlog::error("No images to pack!");
        return false;
    }
    if (!std::filesystem::exists(outputPath)) {
        spdlog::error("Output path does not exist: {}", outputPath.string());
        return false;
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

    auto const packDetailsPath = outputPath / fmt::format("{}.json", filename);
    if (!forceOverwrite && std::filesystem::exists(packDetailsPath)) {
        spdlog::error("Destination exists: {}", packDetailsPath.string());
        return false;
    }

    std::vector<stbrp_node> stbNodes(maxWidth * 2);

    nlohmann::json atlases;
    int numPacks = 0;
    for (; !stbRects.empty(); ++numPacks) {
        auto const imagePngPath = outputPath / fmt::format("{}_{}.png", filename, numPacks);

        if (!forceOverwrite && std::filesystem::exists(imagePngPath)) {
            // previously an error, but downgraded to a warning since we still continue.
            spdlog::warn("Destination exists: {}", imagePngPath.string());
            continue;
        }

        auto const packDim = FindOptimalDimensions(stbNodes, stbRects, { minWidth, minHeight }, { maxWidth, maxHeight });

        stbrp_context stbContext;
        stbrp_init_target(&stbContext, packDim.x, packDim.y, stbNodes.data(), static_cast<int>(stbNodes.size()));
        stbrp_pack_rects(&stbContext, stbRects.data(), static_cast<int>(stbRects.size()));
        auto const atlasJson = CommitPack(imagePngPath, outputPath, dryRun, packDim.x, packDim.y, stbRects, images);
        if (atlasJson.empty()) {
            // empty return from commit pack means something bad happened
            return false;
        }
        atlases.push_back(atlasJson);
    }

    if (!dryRun) {
        std::ofstream ofile(packDetailsPath);
        if (ofile.is_open()) {
            nlohmann::json root;
            root["atlases"] = atlases;
            ofile << root;
        }
    }

    spdlog::info("Packed {} images into {} atlas{}", images.size(), numPacks, numPacks > 1 ? "es" : "");

    return true;
}

} // namespace moth_packer
