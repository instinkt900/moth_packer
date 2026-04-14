#include "moth_packer/packer.h"

#include <glob/glob.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/remove_if.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_rect_pack.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <queue>
#include <unordered_set>
#include <vector>

namespace moth_packer {
    namespace {
        std::unordered_set<std::string> const kSupportedExtensions = {
            ".png", ".jpg", ".jpeg", ".bmp", ".tga"
        };

        // recursively loads all layouts in a given path
        void CollectLayouts(std::filesystem::path const& path,
                            bool recursive,
                            std::vector<std::shared_ptr<moth_ui::Layout>>& layouts) {
            for (auto&& entry : std::filesystem::directory_iterator(path)) {
                if (recursive && std::filesystem::is_directory(entry.path())) {
                    CollectLayouts(entry.path(), recursive, layouts);
                } else if (entry.path().has_extension() &&
                           entry.path().extension() == moth_ui::Layout::FullExtension) {
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
                    if (std::end(images) == ranges::find_if(images, [&](auto const& detail) {
                            return detail.path == imagePath;
                        })) {
                        ImageDetails details;
                        details.path = imagePath;
                        auto const result = stbi_info(imagePath.string().c_str(),
                                                      &details.dimensions.x,
                                                      &details.dimensions.y,
                                                      &details.channels);
                        if (result != 1) {
                            spdlog::warn("Failed to read image info: {}", imagePath.string());
                            continue;
                        }
                        images.push_back(details);
                    }
                }
            }
        }

        std::string LowerExtension(std::filesystem::path const& path) {
            auto ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return ext;
        }

        std::string FormatExtension(AtlasFormat format) {
            switch (format) {
            case AtlasFormat::PNG:  return ".png";
            case AtlasFormat::BMP:  return ".bmp";
            case AtlasFormat::TGA:  return ".tga";
            case AtlasFormat::JPEG: return ".jpg";
            default:
                spdlog::warn("Unknown AtlasFormat value ({}); defaulting to .png",
                             static_cast<int>(format));
                return ".png";
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

        moth_ui::IntVec2 FindOptimalDimensions(std::vector<stbrp_node>& nodes,
                                               std::vector<stbrp_rect>& rects,
                                               moth_ui::IntVec2 const& minPack,
                                               moth_ui::IntVec2 const& maxPack) {
            int64_t totalArea = 0;
            for (auto&& rect : rects) {
                totalArea += static_cast<int64_t>(rect.w) * rect.h;
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
                    int64_t const curArea = static_cast<int64_t>(curWidth) * curHeight;
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
                stbrp_init_target(&stbContext,
                                  testDim.m_dimensions.x,
                                  testDim.m_dimensions.y,
                                  nodes.data(),
                                  static_cast<int>(nodes.size()));
                auto const allPacked =
                    stbrp_pack_rects(&stbContext, rects.data(), static_cast<int>(rects.size()));
                if (allPacked != 0) {
                    float const testArea = static_cast<float>(
                        static_cast<int64_t>(testDim.m_dimensions.x) * testDim.m_dimensions.y);
                    testDim.m_ratio = static_cast<float>(totalArea) / testArea;
                }
            }

            std::sort(std::begin(testDimensions), std::end(testDimensions), [](auto const& a, auto const& b) {
                return b.m_ratio < a.m_ratio;
            });

            // no single dimension fit all rects — use the largest to pack as many as possible
            if (std::begin(testDimensions)->m_ratio == 0.0f) {
                return { maxDimX, maxDimY };
            }

            return std::begin(testDimensions)->m_dimensions;
        }

        // Composites packed rects from in-memory ImageInput pixels into a PackedAtlas.
        // Erases successfully packed rects from the list.
        PackedAtlas CommitPackCore(int width,
                                   int height,
                                   std::vector<stbrp_rect>& rects,
                                   std::vector<ImageInput> const& images,
                                   int padding,
                                   PaddingType paddingType,
                                   uint32_t paddingColor) {
            int const atlasChannels = 4;
            PackedAtlas atlas;
            atlas.width = width;
            atlas.height = height;
            atlas.pixels.resize(static_cast<size_t>(width) * height * atlasChannels);

            // Pre-fill the entire atlas with paddingColor. This sets the background color
            // for the atlas (visible in padding regions and any unpacked areas) and makes
            // the PaddingType::Color border fill a no-op.
            uint8_t const bg_r = static_cast<uint8_t>((paddingColor >> 24) & 0xFF);
            uint8_t const bg_g = static_cast<uint8_t>((paddingColor >> 16) & 0xFF);
            uint8_t const bg_b = static_cast<uint8_t>((paddingColor >> 8) & 0xFF);
            uint8_t const bg_a = static_cast<uint8_t>(paddingColor & 0xFF);
            for (size_t i = 0; i < atlas.pixels.size(); i += atlasChannels) {
                atlas.pixels[i + 0] = bg_r;
                atlas.pixels[i + 1] = bg_g;
                atlas.pixels[i + 2] = bg_b;
                atlas.pixels[i + 3] = bg_a;
            }

            for (auto&& rect : rects) {
                if (rect.was_packed == 0) {
                    continue;
                }

                auto const& img = images[static_cast<size_t>(rect.id)];
                int const srcWidth  = img.width;
                int const srcHeight = img.height;
                uint8_t const* srcPixels = img.pixels.data();

                int const dstX = rect.x + padding;
                int const dstY = rect.y + padding;

                // Blit image into its padded position in the atlas
                for (int row = 0; row < srcHeight; ++row) {
                    auto const srcOffset = static_cast<size_t>(row) * srcWidth * atlasChannels;
                    auto const dstOffset = (static_cast<size_t>(dstY + row) * width + dstX) * atlasChannels;
                    std::memcpy(atlas.pixels.data() + dstOffset,
                                srcPixels + srcOffset,
                                static_cast<size_t>(srcWidth) * atlasChannels);
                }

                // Fill padding border
                if (padding > 0) {
                    auto const srcPixelAt = [&](int sx, int sy) -> uint8_t const* {
                        return srcPixels + (((static_cast<size_t>(sy) * srcWidth) + sx) * atlasChannels);
                    };
                    for (int py = rect.y; py < rect.y + rect.h; ++py) {
                        for (int px = rect.x; px < rect.x + rect.w; ++px) {
                            int const sx = px - dstX;
                            int const sy = py - dstY;
                            if (sx >= 0 && sx < srcWidth && sy >= 0 && sy < srcHeight) {
                                continue; // image area already blitted
                            }
                            auto const dstOffset = (static_cast<size_t>(py) * width + px) * atlasChannels;
                            switch (paddingType) {
                            case PaddingType::Color:
                                // Atlas is pre-filled with paddingColor — nothing to do.
                                break;
                            case PaddingType::Extend: {
                                int const csx = std::clamp(sx, 0, srcWidth - 1);
                                int const csy = std::clamp(sy, 0, srcHeight - 1);
                                std::memcpy(
                                    atlas.pixels.data() + dstOffset, srcPixelAt(csx, csy), atlasChannels);
                                break;
                            }
                            case PaddingType::Mirror: {
                                // Map any coordinate to the mirrored domain using a 2*size period.
                                // This handles padding larger than the image dimension correctly.
                                auto mirrorCoord = [](int c, int size) {
                                    int const period = 2 * size;
                                    c = ((c % period) + period) % period;
                                    return c >= size ? period - c - 1 : c;
                                };
                                std::memcpy(atlas.pixels.data() + dstOffset,
                                            srcPixelAt(mirrorCoord(sx, srcWidth), mirrorCoord(sy, srcHeight)),
                                            atlasChannels);
                                break;
                            }
                            case PaddingType::Wrap: {
                                int const wsx = ((sx % srcWidth) + srcWidth) % srcWidth;
                                int const wsy = ((sy % srcHeight) + srcHeight) % srcHeight;
                                std::memcpy(
                                    atlas.pixels.data() + dstOffset, srcPixelAt(wsx, wsy), atlasChannels);
                                break;
                            }
                            }
                        }
                    }
                }

                atlas.images.push_back(PackedImage{
                    img.name,
                    moth_ui::MakeRect(dstX, dstY, srcWidth, srcHeight)
                });
            }

            rects.erase(ranges::remove_if(rects, [](auto const& r) { return r.was_packed != 0; }),
                        std::end(rects));

            return atlas;
        }

        // Writes a PackedAtlas pixel buffer to disk in the requested format.
        // Returns true on success (or when dryRun is true).
        bool WriteAtlasImage(std::filesystem::path const& path,
                             PackedAtlas const& atlas,
                             bool dryRun,
                             AtlasFormat format,
                             int jpegQuality) {
            if (dryRun) {
                return true;
            }
            int const channels = 4;
            auto const pathStr = path.string();
            int writeResult = 0;
            switch (format) {
            case AtlasFormat::PNG:
                writeResult = stbi_write_png(pathStr.c_str(), atlas.width, atlas.height, channels,
                                             atlas.pixels.data(), atlas.width * channels);
                break;
            case AtlasFormat::BMP:
                writeResult = stbi_write_bmp(pathStr.c_str(), atlas.width, atlas.height, channels,
                                             atlas.pixels.data());
                break;
            case AtlasFormat::TGA:
                writeResult = stbi_write_tga(pathStr.c_str(), atlas.width, atlas.height, channels,
                                             atlas.pixels.data());
                break;
            case AtlasFormat::JPEG:
                writeResult = stbi_write_jpg(pathStr.c_str(), atlas.width, atlas.height, channels,
                                             atlas.pixels.data(), jpegQuality);
                break;
            }
            if (writeResult == 0) {
                spdlog::error("Failed to write atlas image: {}", path.string());
                return false;
            }
            return true;
        }

        std::string LoopTypeStr(LoopType t) {
            switch (t) {
            case LoopType::Loop:  return "loop";
            case LoopType::Stop:  return "stop";
            case LoopType::Reset: return "reset";
            default:              return "loop";
            }
        }
    } // anonymous namespace

    bool CollectImagesFromFile(std::filesystem::path const& inputList, std::vector<ImageDetails>& dstList) {
        if (!std::filesystem::exists(inputList)) {
            spdlog::error("Input path does not exist: {}", inputList.string());
            return false;
        }

        std::vector<ImageDetails> images;

        std::ifstream file(inputList);
        if (!file.is_open()) {
            spdlog::error("Failed to open input list: {} ({})", inputList.string(), strerror(errno));
            return false;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) {
                continue;
            }
            std::filesystem::path imagePath(line);
            auto const ext = LowerExtension(imagePath);
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
            if (stbi_info(imagePath.string().c_str(),
                          &details.dimensions.x,
                          &details.dimensions.y,
                          &details.channels) != 1) {
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

    bool CollectImagesFromGlob(std::string const& pattern, std::vector<ImageDetails>& dstList) {
        std::vector<ImageDetails> images;
        for (auto&& matchPath : glob::rglob(pattern)) {
            auto const ext = LowerExtension(matchPath);
            if (kSupportedExtensions.find(ext) == std::end(kSupportedExtensions)) {
                spdlog::warn("Unsupported image format, skipping: {}", matchPath.string());
                continue;
            }
            auto const isDuplicate = [&](ImageDetails const& d) { return d.path == matchPath; };
            if (ranges::find_if(dstList, isDuplicate) != std::end(dstList) ||
                ranges::find_if(images, isDuplicate) != std::end(images)) {
                continue;
            }
            ImageDetails details;
            details.path = matchPath;
            if (stbi_info(matchPath.string().c_str(),
                          &details.dimensions.x,
                          &details.dimensions.y,
                          &details.channels) != 1) {
                spdlog::warn("Failed to read image info: {}", matchPath.string());
                continue;
            }
            images.push_back(details);
        }

        if (images.empty()) {
            spdlog::info("No images matched glob: {}", pattern);
            return false;
        }

        dstList.insert(std::end(dstList), std::begin(images), std::end(images));
        return true;
    }

    bool CollectImagesFromDir(std::filesystem::path const& inputPath,
                              bool recursive,
                              std::vector<ImageDetails>& dstList) {
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
                auto const ext = LowerExtension(entry.path());
                if (kSupportedExtensions.find(ext) == std::end(kSupportedExtensions)) {
                    spdlog::warn("Unsupported image format, skipping: {}", entry.path().string());
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
                if (stbi_info(imagePath.string().c_str(),
                              &details.dimensions.x,
                              &details.dimensions.y,
                              &details.channels) != 1) {
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

    bool CollectImagesFromLayout(std::filesystem::path const& inputLayout,
                                 std::vector<ImageDetails>& dstList) {
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

    bool CollectImagesFromLayoutsDir(std::filesystem::path const& inputPath,
                                     bool recursive,
                                     std::vector<ImageDetails>& dstList) {
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

    PackResult PackToMemory(std::vector<ImageInput> images, PackOptions const& options) {
        if (images.empty()) {
            spdlog::error("No images to pack!");
            return {};
        }
        if (options.padding < 0) {
            spdlog::error("PackOptions::padding must be non-negative (got {})", options.padding);
            return {};
        }
        if (options.minWidth <= 0 || options.minHeight <= 0) {
            spdlog::error("PackOptions::minWidth/minHeight must be positive");
            return {};
        }
        if (options.maxWidth <= 0 || options.maxHeight <= 0) {
            spdlog::error("PackOptions::maxWidth/maxHeight must be positive");
            return {};
        }
        if (options.maxWidth < options.minWidth || options.maxHeight < options.minHeight) {
            spdlog::error("PackOptions::maxWidth/maxHeight must be >= minWidth/minHeight");
            return {};
        }
        if (options.format == AtlasFormat::JPEG &&
            (options.jpegQuality < 1 || options.jpegQuality > 100)) {
            spdlog::error("PackOptions::jpegQuality must be in the range 1-100 (got {})", options.jpegQuality);
            return {};
        }
        // Guard against overflow in maxWidth*2 (stbNodes) and unrealistic atlas sizes.
        constexpr int kMaxAtlasDim = 65536;
        if (options.maxWidth > kMaxAtlasDim || options.maxHeight > kMaxAtlasDim) {
            spdlog::error("PackOptions::maxWidth/maxHeight must not exceed {}", kMaxAtlasDim);
            return {};
        }

        for (auto const& img : images) {
            if (img.width <= 0 || img.height <= 0) {
                spdlog::error("ImageInput '{}' has invalid dimensions ({}x{})", img.name, img.width, img.height);
                return {};
            }
            auto const expectedSize = static_cast<size_t>(img.width) * img.height * 4;
            if (img.pixels.size() != expectedSize) {
                spdlog::error("ImageInput '{}' pixel data size mismatch (got {}, expected {})",
                              img.name, img.pixels.size(), expectedSize);
                return {};
            }
        }

        // ---- Flipbook path ----
        if (options.packType == PackType::Flipbook) {
            if (options.fps <= 0) {
                spdlog::error("PackOptions::fps must be positive for flipbook output (got {})", options.fps);
                return {};
            }

            // Sort alphabetically by the filename component of the name so frames can be
            // numbered: 001.png, 002.png, etc. Using the filename component rather than the
            // full name means directory prefixes (when Pack() builds names from full paths)
            // do not affect ordering.
            std::sort(images.begin(), images.end(), [](ImageInput const& a, ImageInput const& b) {
                return std::filesystem::path(a.name).filename() <
                       std::filesystem::path(b.name).filename();
            });

            std::vector<stbrp_rect> stbRects;
            stbRects.reserve(images.size());
            for (int i = 0; i < static_cast<int>(images.size()); ++i) {
                stbrp_rect r;
                r.id = i;
                r.w = images[i].width + (options.padding * 2);
                r.h = images[i].height + (options.padding * 2);
                stbRects.push_back(r);
            }

            std::vector<stbrp_node> stbNodes(options.maxWidth * 2);
            auto const packDim = FindOptimalDimensions(stbNodes, stbRects,
                                                        { options.minWidth, options.minHeight },
                                                        { options.maxWidth, options.maxHeight });

            stbrp_context stbContext;
            stbrp_init_target(&stbContext, packDim.x, packDim.y,
                              stbNodes.data(), static_cast<int>(stbNodes.size()));
            stbrp_pack_rects(&stbContext, stbRects.data(), static_cast<int>(stbRects.size()));

            // All frames must fit in a single atlas for a valid flipbook.
            for (auto const& r : stbRects) {
                if (r.was_packed == 0) {
                    spdlog::error("Flipbook: '{}' could not be packed into a single atlas "
                                  "(try increasing maxWidth/maxHeight)",
                                  images[static_cast<size_t>(r.id)].name);
                    return {};
                }
            }

            // Capture per-frame rects before CommitPackCore erases them.
            std::vector<PackedFrame> frames(images.size());
            for (auto const& r : stbRects) {
                auto const idx = static_cast<size_t>(r.id);
                frames[idx].rect = moth_ui::MakeRect(r.x + options.padding, r.y + options.padding,
                                                      images[idx].width, images[idx].height);
                // pivot stays zero-initialised
            }

            // CommitPackCore erases packed rects — pass a copy so stbRects stays valid above.
            auto rectsForCommit = stbRects;
            auto atlas = CommitPackCore(packDim.x, packDim.y, rectsForCommit, images,
                                        options.padding, options.paddingType, options.paddingColor);
            // In flipbook mode frame data lives in PackResult::frames, not atlas.images.
            atlas.images.clear();

            // Distribute frame durations using Bresenham error accumulation so the sum of all
            // step durations equals round(frameCount * 1000.0 / fps), preserving average
            // playback speed even when 1000 is not evenly divisible by fps.
            double const exactMs = 1000.0 / options.fps;
            PackedClip clip;
            clip.name = options.clipName.empty() ? "default" : options.clipName;
            clip.loop = options.loop;
            clip.steps.reserve(images.size());
            double durationError = 0.0;
            for (int i = 0; i < static_cast<int>(images.size()); ++i) {
                double const target = exactMs + durationError;
                int const durationMs = static_cast<int>(std::round(target));
                durationError = target - durationMs;
                clip.steps.push_back({ i, durationMs });
            }

            PackResult result;
            result.atlases.push_back(std::move(atlas));
            result.frames = std::move(frames);
            result.clips.push_back(std::move(clip));

            spdlog::info("Packed flipbook: {} frames, {}x{} px",
                         result.frames.size(), packDim.x, packDim.y);
            return result;
        }

        // ---- Atlas path ----
        std::vector<stbrp_rect> stbRects;
        stbRects.reserve(images.size());
        for (int i = 0; i < static_cast<int>(images.size()); ++i) {
            stbrp_rect r;
            r.id = i;
            r.w = images[i].width + (options.padding * 2);
            r.h = images[i].height + (options.padding * 2);
            if (r.w <= options.maxWidth && r.h <= options.maxHeight) {
                stbRects.push_back(r);
            } else {
                spdlog::warn("Image '{}' exceeds max atlas size, skipping", images[i].name);
            }
        }

        if (stbRects.empty()) {
            spdlog::error("No images could be packed (all images exceeded max atlas dimensions)");
            return {};
        }

        std::vector<stbrp_node> stbNodes(options.maxWidth * 2);
        PackResult result;

        while (!stbRects.empty()) {
            auto const packDim = FindOptimalDimensions(stbNodes, stbRects,
                                                        { options.minWidth, options.minHeight },
                                                        { options.maxWidth, options.maxHeight });

            stbrp_context stbContext;
            stbrp_init_target(&stbContext, packDim.x, packDim.y,
                              stbNodes.data(), static_cast<int>(stbNodes.size()));
            stbrp_pack_rects(&stbContext, stbRects.data(), static_cast<int>(stbRects.size()));

            result.atlases.push_back(CommitPackCore(packDim.x, packDim.y, stbRects, images,
                                                     options.padding, options.paddingType, options.paddingColor));
        }

        spdlog::info("Packed {} images into {} atlas{}",
                     images.size(), result.atlases.size(), result.atlases.size() > 1 ? "es" : "");
        return result;
    }

    bool Pack(std::vector<ImageDetails> imageDetails, PackOptions const& options) {
        if (imageDetails.empty()) {
            spdlog::error("No images to pack!");
            return false;
        }
        if (options.filename.empty()) {
            spdlog::error("PackOptions::filename must not be empty");
            return false;
        }

        // Load all images from disk into in-memory pixel buffers.
        constexpr int kChannels = 4;
        std::vector<ImageInput> inputs;
        inputs.reserve(imageDetails.size());
        for (auto const& detail : imageDetails) {
            int srcWidth = 0;
            int srcHeight = 0;
            int srcChannels = 0;
            stbi_uc* pixels = stbi_load(detail.path.string().c_str(), &srcWidth, &srcHeight, &srcChannels, kChannels);
            if (pixels == nullptr) {
                spdlog::error("Failed to load image: {}", detail.path.string());
                return false;
            }
            ImageInput input;
            input.name = detail.path.string();
            input.width = srcWidth;
            input.height = srcHeight;
            auto const pixelCount = static_cast<size_t>(srcWidth) * srcHeight * kChannels;
            input.pixels.assign(pixels, pixels + pixelCount);
            stbi_image_free(pixels);
            inputs.push_back(std::move(input));
        }

        auto result = PackToMemory(std::move(inputs), options);
        if (result.atlases.empty()) {
            return false;
        }

        if (!options.dryRun) {
            try {
                std::filesystem::create_directories(options.outputPath);
            } catch (std::filesystem::filesystem_error const& e) {
                spdlog::error("Failed to create output directory '{}': {}",
                              options.outputPath.string(), e.what());
                return false;
            }
        }

        std::string const ext = FormatExtension(options.format);

        // ---- Flipbook path ----
        if (options.packType == PackType::Flipbook) {
            auto const atlasPath = options.outputPath / (options.filename + ext);
            auto const jsonPath  = options.outputPath / (options.filename + ".flipbook.json");

            if (!options.forceOverwrite) {
                if (std::filesystem::exists(atlasPath)) {
                    spdlog::error("Output already exists (use --force to overwrite): {}", atlasPath.string());
                    return false;
                }
                if (std::filesystem::exists(jsonPath)) {
                    spdlog::error("Output already exists (use --force to overwrite): {}", jsonPath.string());
                    return false;
                }
            }

            if (!WriteAtlasImage(atlasPath, result.atlases[0], options.dryRun, options.format, options.jpegQuality)) {
                return false;
            }

            auto const recordedAtlas = options.absolutePaths
                ? std::filesystem::absolute(atlasPath).string()
                : std::filesystem::relative(atlasPath, options.outputPath).string();

            nlohmann::json j;
            j["image"]  = recordedAtlas;
            j["frames"] = nlohmann::json::array();
            for (auto const& frame : result.frames) {
                nlohmann::json frameJson;
                frameJson["x"]       = frame.rect.x();
                frameJson["y"]       = frame.rect.y();
                frameJson["w"]       = frame.rect.w();
                frameJson["h"]       = frame.rect.h();
                frameJson["pivot_x"] = frame.pivot.x;
                frameJson["pivot_y"] = frame.pivot.y;
                j["frames"].push_back(frameJson);
            }

            j["clips"] = nlohmann::json::array();
            for (auto const& clip : result.clips) {
                nlohmann::json clipJson;
                clipJson["name"]   = clip.name;
                clipJson["loop"]   = LoopTypeStr(clip.loop);
                clipJson["frames"] = nlohmann::json::array();
                for (auto const& step : clip.steps) {
                    nlohmann::json stepJson;
                    stepJson["frame"]       = step.frameIndex;
                    stepJson["duration_ms"] = step.durationMs;
                    clipJson["frames"].push_back(stepJson);
                }
                j["clips"].push_back(clipJson);
            }

            if (!options.dryRun) {
                std::ofstream out(jsonPath);
                if (!out.is_open()) {
                    spdlog::error("Failed to write flipbook descriptor: {}", jsonPath.string());
                    return false;
                }
                out << (options.prettyJson ? j.dump(4) : j.dump());
                out.flush();
                if (!out) {
                    spdlog::error("Failed to write flipbook descriptor: {}", jsonPath.string());
                    return false;
                }
            }

            spdlog::info("Wrote flipbook: {} ({} frames, {}x{} px)",
                         jsonPath.string(), result.frames.size(),
                         result.atlases[0].width, result.atlases[0].height);
            return true;
        }

        // ---- Atlas path ----
        auto const packDetailsPath = options.outputPath / fmt::format("{}.json", options.filename);
        if (!options.forceOverwrite && std::filesystem::exists(packDetailsPath)) {
            spdlog::error("Destination exists: {}", packDetailsPath.string());
            return false;
        }

        nlohmann::json atlases;
        for (size_t atlasIdx = 0; atlasIdx < result.atlases.size(); ++atlasIdx) {
            auto const& atlas = result.atlases[atlasIdx];
            auto const atlasImagePath =
                options.outputPath / fmt::format("{}_{}{}", options.filename, atlasIdx, ext);

            if (!options.forceOverwrite && std::filesystem::exists(atlasImagePath)) {
                spdlog::error("Output already exists (use --force to overwrite): {}", atlasImagePath.string());
                return false;
            }

            if (!WriteAtlasImage(atlasImagePath, atlas, options.dryRun, options.format, options.jpegQuality)) {
                return false;
            }

            nlohmann::json atlasEntry;
            atlasEntry["atlas"] = (options.absolutePaths
                ? std::filesystem::absolute(atlasImagePath)
                : std::filesystem::relative(atlasImagePath, options.outputPath)).string();

            nlohmann::json atlasImages;
            for (auto const& img : atlas.images) {
                auto const imagePath = std::filesystem::path(img.name);
                auto const recordedPath = options.absolutePaths
                    ? std::filesystem::absolute(imagePath)
                    : std::filesystem::relative(imagePath, options.outputPath);
                nlohmann::json details;
                details["path"] = recordedPath.string();
                details["rect"] = {
                    { "x", img.rect.x() }, { "y", img.rect.y() },
                    { "w", img.rect.w() }, { "h", img.rect.h() }
                };
                atlasImages.push_back(details);
                spdlog::info("Packed {} into {}", recordedPath.string(), atlasImagePath.string());
            }
            atlasEntry["images"] = atlasImages;
            atlases.push_back(atlasEntry);
        }

        if (!options.dryRun) {
            std::ofstream ofile(packDetailsPath);
            if (!ofile.is_open()) {
                spdlog::error("Failed to open descriptor for writing: {}", packDetailsPath.string());
                return false;
            }
            nlohmann::json root;
            root["atlases"] = atlases;
            ofile << (options.prettyJson ? root.dump(4) : root.dump());
            ofile.flush();
            if (ofile.fail()) {
                spdlog::error("Failed to write descriptor: {}", packDetailsPath.string());
                return false;
            }
        }

        spdlog::info("Packed {} images into {} atlas{}",
                     imageDetails.size(), result.atlases.size(), result.atlases.size() > 1 ? "es" : "");
        return true;
    }

    bool Unpack(std::filesystem::path const& sheetPath, UnpackOptions const& options) {
        if (!std::filesystem::exists(sheetPath)) {
            spdlog::error("Sprite sheet does not exist: {}", sheetPath.string());
            return false;
        }
        if (options.format == AtlasFormat::JPEG &&
            (options.jpegQuality < 1 || options.jpegQuality > 100)) {
            spdlog::error("UnpackOptions::jpegQuality must be in the range 1-100 (got {})", options.jpegQuality);
            return false;
        }

        if (!options.dryRun) {
            try {
                std::filesystem::create_directories(options.outputPath);
            } catch (std::filesystem::filesystem_error const& e) {
                spdlog::error("Failed to create output directory '{}': {}",
                              options.outputPath.string(), e.what());
                return false;
            }
        }

        constexpr int kChannels = 4;
        int width = 0;
        int height = 0;
        int srcChannels = 0;
        stbi_uc* pixels = stbi_load(sheetPath.string().c_str(), &width, &height, &srcChannels, kChannels);
        if (pixels == nullptr) {
            spdlog::error("Failed to load sprite sheet: {}", sheetPath.string());
            return false;
        }

        // Resolve which background color to use for sprite detection.
        // Priority: explicit backgroundColor > autoDetectBackground > alpha threshold.
        std::optional<std::array<uint8_t, 3>> bgColor = options.backgroundColor;
        if (!bgColor.has_value() && options.autoDetectBackground) {
            // Sample the four corners. If they agree within colorThreshold on every
            // channel, use their average as the background color.
            struct Corner { int x; int y; };
            std::array<Corner, 4> const corners{{ {0, 0}, {width - 1, 0}, {0, height - 1}, {width - 1, height - 1} }};
            std::array<std::array<uint8_t, 3>, 4> samples{};
            for (size_t i = 0; i < corners.size(); ++i) {
                size_t const base = (static_cast<size_t>(corners[i].y) * width + corners[i].x) * kChannels;
                samples[i] = { pixels[base], pixels[base + 1], pixels[base + 2] };
            }
            bool cornersAgree = true;
            for (int ch = 0; ch < 3 && cornersAgree; ++ch) {
                uint8_t lo = samples[0][ch];
                uint8_t hi = samples[0][ch];
                for (size_t i = 1; i < samples.size(); ++i) {
                    lo = std::min(lo, samples[i][ch]);
                    hi = std::max(hi, samples[i][ch]);
                }
                if ((hi - lo) > options.colorThreshold) {
                    cornersAgree = false;
                }
            }
            if (cornersAgree) {
                std::array<uint8_t, 3> avg{};
                for (int ch = 0; ch < 3; ++ch) {
                    int sum = 0;
                    for (auto const& s : samples) { sum += s[ch]; }
                    avg[ch] = static_cast<uint8_t>(sum / 4);
                }
                bgColor = avg;
                spdlog::info("Auto-detected background color: #{:02X}{:02X}{:02X}",
                             avg[0], avg[1], avg[2]);
            } else {
                spdlog::warn("Corner pixels disagree; falling back to alpha-based detection");
            }
        }

        // Sprite detection uses a BFS flood fill over the pixel grid.
        // A pixel is "active" (part of a sprite) when it is not background:
        //   - color mode: any RGB channel differs from bgColor by more than colorThreshold
        //   - alpha mode:  alpha channel exceeds alphaThreshold
        // Each BFS seed finds one connected component (sprite), using
        // 8-connectivity so diagonally touching pixels belong to the same sprite.
        // The bounding rect of each component is recorded; pixels already
        // visited by a previous BFS are skipped so each sprite is found once.
        std::vector<bool> visited(static_cast<size_t>(width) * height, false);

        auto isActive = [&](int x, int y) -> bool {
            size_t const base = ((static_cast<size_t>(y) * width + x) * kChannels);
            if (bgColor.has_value()) {
                auto const& bg = bgColor.value();
                return std::abs(int{pixels[base + 0]} - int{bg[0]}) > options.colorThreshold ||
                       std::abs(int{pixels[base + 1]} - int{bg[1]}) > options.colorThreshold ||
                       std::abs(int{pixels[base + 2]} - int{bg[2]}) > options.colorThreshold;
            }
            return pixels[base + 3] > options.alphaThreshold;
        };

        struct SpriteRect { int x, y, w, h; };
        std::vector<SpriteRect> sprites;

        // All 8 neighbors: cardinal (N/S/E/W) + diagonal.
        static constexpr std::array<std::pair<int, int>, 8> kNeighbors{{
            {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
        }};

        // Scan top-to-bottom, left-to-right. The first unvisited active pixel
        // seeds a BFS that consumes the entire connected component.
        for (int sy = 0; sy < height; ++sy) {
            for (int sx = 0; sx < width; ++sx) {
                size_t const idx = (static_cast<size_t>(sy) * width) + sx;
                if (visited[idx] || !isActive(sx, sy)) {
                    continue;
                }

                // BFS: expand outward from the seed pixel, tracking the
                // axis-aligned bounding box as we go.
                std::queue<std::pair<int, int>> q;
                q.push({sx, sy});
                visited[idx] = true;
                int minX = sx;
                int maxX = sx;
                int minY = sy;
                int maxY = sy;

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    minX = std::min(minX, cx);
                    maxX = std::max(maxX, cx);
                    minY = std::min(minY, cy);
                    maxY = std::max(maxY, cy);

                    for (auto [dx, dy] : kNeighbors) {
                        int const nx = cx + dx;
                        int const ny = cy + dy;
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                            continue;
                        }
                        size_t const nidx = (static_cast<size_t>(ny) * width) + nx;
                        if (visited[nidx] || !isActive(nx, ny)) {
                            continue;
                        }
                        visited[nidx] = true;
                        q.push({nx, ny});
                    }
                }

                // Convert inclusive max coords to width/height and store.
                sprites.push_back({minX, minY, maxX - minX + 1, maxY - minY + 1});
            }
        }

        spdlog::info("Found {} sprite{} in {}",
                     sprites.size(),
                     sprites.size() != 1 ? "s" : "",
                     sheetPath.filename().string());

        // Filter by size bounds if specified (0 means no bound).
        if (options.minSpriteWidth > 0 || options.minSpriteHeight > 0 ||
            options.maxSpriteWidth > 0 || options.maxSpriteHeight > 0) {
            size_t const before = sprites.size();
            sprites.erase(std::remove_if(sprites.begin(), sprites.end(), [&](SpriteRect const& r) {
                if (options.minSpriteWidth  > 0 && r.w < options.minSpriteWidth)  { return true; }
                if (options.minSpriteHeight > 0 && r.h < options.minSpriteHeight) { return true; }
                if (options.maxSpriteWidth  > 0 && r.w > options.maxSpriteWidth)  { return true; }
                if (options.maxSpriteHeight > 0 && r.h > options.maxSpriteHeight) { return true; }
                return false;
            }), sprites.end());
            size_t const filtered = before - sprites.size();
            if (filtered > 0) {
                spdlog::info("Filtered out {} sprite{} outside size bounds",
                             filtered, filtered != 1 ? "s" : "");
            }
        }

        bool success = true;
        for (size_t i = 0; i < sprites.size(); ++i) {
            auto const& r = sprites[i];
            auto const outPath = options.outputPath /
                fmt::format("{}_{}{}", options.spritePrefix, i, FormatExtension(options.format));

            if (!options.forceOverwrite && std::filesystem::exists(outPath)) {
                spdlog::error("Destination exists: {}", outPath.string());
                success = false;
                continue;
            }

            if (!options.dryRun) {
                std::vector<stbi_uc> spritePixels(static_cast<size_t>(r.w) * r.h * kChannels);
                if (options.replaceBackgroundColor.has_value()) {
                    // Per-pixel copy: replace background pixels with the replacement color.
                    uint32_t const rep = options.replaceBackgroundColor.value();
                    uint8_t const repR = static_cast<uint8_t>((rep >> 24) & 0xFF);
                    uint8_t const repG = static_cast<uint8_t>((rep >> 16) & 0xFF);
                    uint8_t const repB = static_cast<uint8_t>((rep >>  8) & 0xFF);
                    uint8_t const repA = static_cast<uint8_t>( rep        & 0xFF);
                    for (int row = 0; row < r.h; ++row) {
                        for (int col = 0; col < r.w; ++col) {
                            auto const srcOff = (static_cast<size_t>(r.y + row) * width + r.x + col) * kChannels;
                            auto const dstOff = (static_cast<size_t>(row) * r.w + col) * kChannels;
                            if (isActive(r.x + col, r.y + row)) {
                                std::memcpy(spritePixels.data() + dstOff, pixels + srcOff, kChannels);
                            } else {
                                spritePixels[dstOff + 0] = repR;
                                spritePixels[dstOff + 1] = repG;
                                spritePixels[dstOff + 2] = repB;
                                spritePixels[dstOff + 3] = repA;
                            }
                        }
                    }
                } else {
                    // Fast path: copy the bounding-rect rows one at a time.
                    for (int row = 0; row < r.h; ++row) {
                        auto const srcOff = (static_cast<size_t>(r.y + row) * width + r.x) * kChannels;
                        auto const dstOff = static_cast<size_t>(row) * r.w * kChannels;
                        std::memcpy(spritePixels.data() + dstOff, pixels + srcOff,
                                    static_cast<size_t>(r.w) * kChannels);
                    }
                }

                auto const pathStr = outPath.string();
                auto const* pathCStr = pathStr.c_str();
                int writeResult = 0;
                switch (options.format) {
                case AtlasFormat::PNG:
                    writeResult = stbi_write_png(pathCStr, r.w, r.h, kChannels,
                                                 spritePixels.data(), r.w * kChannels);
                    break;
                case AtlasFormat::BMP:
                    writeResult = stbi_write_bmp(pathCStr, r.w, r.h, kChannels, spritePixels.data());
                    break;
                case AtlasFormat::TGA:
                    writeResult = stbi_write_tga(pathCStr, r.w, r.h, kChannels, spritePixels.data());
                    break;
                case AtlasFormat::JPEG:
                    writeResult = stbi_write_jpg(pathCStr, r.w, r.h, kChannels,
                                                 spritePixels.data(), options.jpegQuality);
                    break;
                default:
                    spdlog::warn("Unknown AtlasFormat value; defaulting to PNG");
                    writeResult = stbi_write_png(pathCStr, r.w, r.h, kChannels,
                                                 spritePixels.data(), r.w * kChannels);
                    break;
                }

                if (writeResult == 0) {
                    spdlog::error("Failed to write sprite: {}", outPath.string());
                    success = false;
                    continue;
                }
            }

            spdlog::info("Extracted sprite {} ({}x{}) to {}", i, r.w, r.h, outPath.string());
        }

        stbi_image_free(pixels);
        return success;
    }

} // namespace moth_packer
