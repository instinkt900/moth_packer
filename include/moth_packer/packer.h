#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <moth_ui/moth_ui.h>

namespace moth_packer {

    /// Metadata for a single image to be packed from a file path.
    struct ImageDetails {
        std::filesystem::path path;
        moth_ui::IntVec2 dimensions;
        int channels = 0;
    };

    /// A single image to be packed, supplied as raw pixel data.
    /// Use with PackToMemory() when images are already resident in memory.
    struct ImageInput {
        std::string name;              ///< Logical name used in the output descriptor (e.g. a filename stem or asset key).
        int width = 0;                 ///< Image width in pixels.
        int height = 0;                ///< Image height in pixels.
        std::vector<uint8_t> pixels;   ///< Raw RGBA pixel data, exactly width * height * 4 bytes.
    };

    /// Output image format for atlas files.
    enum class AtlasFormat {
        PNG,   ///< PNG (lossless, supports alpha).
        BMP,   ///< BMP (uncompressed, supports alpha).
        TGA,   ///< TGA (supports alpha, optional RLE).
        JPEG,  ///< JPEG (lossy, no alpha; see PackOptions::jpegQuality).
    };

    /// Strategy used to fill the padding border around each packed image.
    enum class PaddingType {
        Color,  ///< Fill with a solid color (see PackOptions::paddingColor).
        Extend, ///< Repeat the nearest edge pixel outward.
        Mirror, ///< Reflect pixels across each edge.
        Wrap,   ///< Tile pixels from the opposite edge.
    };

    /// Loop behavior for a flipbook clip.
    enum class LoopType {
        Loop,   ///< Jump back to the first frame and keep playing indefinitely.
        Stop,   ///< Freeze on the last frame and fire EventFlipbookStopped.
        Reset,  ///< Rewind to the first frame, freeze, and fire EventFlipbookStopped.
    };

    /// Output type for the pack operation.
    enum class PackType {
        Atlas,    ///< Standard texture atlas: multi-atlas JSON with image path/rect entries.
        Flipbook, ///< Flipbook: single-atlas JSON with a frames array and named clip sequences.
    };

    /// A single image placed in an atlas, returned by PackToMemory().
    struct PackedImage {
        std::string name;         ///< The logical name from ImageInput::name.
        moth_ui::IntRect rect;    ///< Pixel rect within the atlas (excludes padding).
    };

    /// A single atlas produced by PackToMemory().
    /// In atlas mode there may be more than one. In flipbook mode there is exactly one.
    struct PackedAtlas {
        int width = 0;                    ///< Atlas width in pixels.
        int height = 0;                   ///< Atlas height in pixels.
        std::vector<uint8_t> pixels;      ///< Raw RGBA pixel data, width * height * 4 bytes.
        std::vector<PackedImage> images;  ///< Images packed into this atlas (atlas mode). Empty in flipbook mode.
    };

    /// Per-frame pixel rect and pivot for a flipbook result.
    struct PackedFrame {
        moth_ui::IntRect rect;   ///< Pixel rect within the atlas (excludes padding).
        moth_ui::IntVec2 pivot;  ///< Frame pivot point in pixels (zero for auto-generated flipbooks).
    };

    /// A single step (frame reference + duration) within a flipbook clip.
    struct PackedClipStep {
        int frameIndex = 0;  ///< Index into PackResult::frames.
        int durationMs = 0;  ///< Duration of this step in milliseconds.
    };

    /// A named clip sequence in a flipbook result.
    struct PackedClip {
        std::string name;
        LoopType loop = LoopType::Loop;
        std::vector<PackedClipStep> steps;
    };

    /// Result returned by PackToMemory(). A non-empty atlases vector indicates success.
    struct PackResult {
        std::vector<PackedAtlas> atlases; ///< One or more atlases (atlas mode); exactly one (flipbook mode).
        std::vector<PackedFrame> frames;  ///< Per-frame rects/pivots (flipbook mode only; empty for atlas mode).
        std::vector<PackedClip>  clips;   ///< Named clips (flipbook mode only; empty for atlas mode).
    };

    /// @brief Collect images from a text file containing one image path per line.
    /// @param inputList Path to the text file.
    /// @param dstList   List to append discovered images to. Duplicates are skipped.
    /// @return True if at least one image was added.
    bool CollectImagesFromFile(std::filesystem::path const& inputList, std::vector<ImageDetails>& dstList);

    /// @brief Collect images whose paths match a glob pattern (e.g. "assets/**/*.png").
    /// @param pattern Glob pattern. Quote the argument on the command line to prevent shell expansion.
    /// @param dstList List to append discovered images to. Duplicates are skipped.
    /// @return True if at least one image was added.
    bool CollectImagesFromGlob(std::string const& pattern, std::vector<ImageDetails>& dstList);

    /// @brief Collect images from a directory.
    /// @param inputPath Path to the directory.
    /// @param recursive Whether to recurse into subdirectories.
    /// @param dstList   List to append discovered images to. Duplicates are skipped.
    /// @return True if at least one image was added.
    bool CollectImagesFromDir(std::filesystem::path const& inputPath,
                              bool recursive,
                              std::vector<ImageDetails>& dstList);

    /// @brief Collect images referenced by a single moth_ui layout file.
    /// @param inputLayout Path to the layout file.
    /// @param dstList     List to append discovered images to. Duplicates are skipped.
    /// @return True if at least one image was added.
    bool CollectImagesFromLayout(std::filesystem::path const& inputLayout,
                                 std::vector<ImageDetails>& dstList);

    /// @brief Collect images referenced by all moth_ui layout files in a directory.
    /// @param inputPath Path to the directory.
    /// @param recursive Whether to recurse into subdirectories.
    /// @param dstList   List to append discovered images to. Duplicates are skipped.
    /// @return True if at least one image was added.
    bool CollectImagesFromLayoutsDir(std::filesystem::path const& inputPath,
                                     bool recursive,
                                     std::vector<ImageDetails>& dstList);

    /// Options controlling atlas/flipbook generation.
    /// @note `outputPath` and `filename` are required — Pack() will return false if either is empty.
    struct PackOptions {
        std::filesystem::path outputPath;           ///< Directory where output files are written.
        std::string filename;                       ///< Base name for output files (no extension).
        bool forceOverwrite = false;                ///< Overwrite existing output files without error.
        bool dryRun = false;                        ///< Run the full packing pipeline but do not write any files.
        int minWidth = 256;                         ///< Minimum atlas width (rounded up to the next power of two).
        int minHeight = 256;                        ///< Minimum atlas height (rounded up to the next power of two).
        int maxWidth = 4096;                        ///< Maximum atlas width (rounded up to the next power of two).
        int maxHeight = 4096;                       ///< Maximum atlas height (rounded up to the next power of two).
        int padding = 0;                            ///< Pixels of padding added around each image on all sides.
        PaddingType paddingType = PaddingType::Color; ///< How the padding border pixels are filled.
        uint32_t paddingColor = 0;                  ///< Atlas background color as RRGGBBAA.
        bool prettyJson = false;                    ///< Pretty-print the JSON descriptor with 4-space indentation.
        bool absolutePaths = false;                 ///< Write absolute paths in the JSON descriptor.
        AtlasFormat format = AtlasFormat::PNG;      ///< Output image format for atlas files.
        int jpegQuality = 90;                       ///< JPEG encode quality (1–100).

        /// Selects what kind of output to produce.
        PackType packType = PackType::Atlas;

        /// Flipbook options — only used when packType == PackType::Flipbook.
        int fps = 12;                          ///< Frames per second for the default clip.
        LoopType loop = LoopType::Loop;        ///< Loop behavior for the default clip.
        std::string clipName = "default";      ///< Name of the auto-generated clip.
    };

    /// @brief Pack images into one or more atlases (or a single flipbook atlas) entirely in memory.
    ///
    /// This is the core packing function. Pack(ImageDetails, PackOptions) is a convenience wrapper
    /// that loads images from disk, calls PackToMemory, then writes the resulting pixels and JSON
    /// descriptor to the output directory.
    ///
    /// When `options.packType == PackType::Atlas`:
    ///   Images are bin-packed into the smallest power-of-two atlas that achieves the best area
    ///   utilisation. Multiple atlases are produced if needed. Each atlas in the result contains
    ///   the composited pixel data and a list of packed images with their rects.
    ///
    /// When `options.packType == PackType::Flipbook`:
    ///   Images are sorted alphabetically by name and bin-packed into a single atlas. The result
    ///   contains one atlas, a `frames` array with per-frame rects and pivots, and a `clips` array
    ///   with a single auto-generated clip covering all frames in order. Returns an empty result if
    ///   not all images fit into one atlas — increase `maxWidth`/`maxHeight` in that case.
    ///
    /// `options.outputPath` and `options.filename` are not used by this function.
    ///
    /// @param images  Images to pack. Each must have a valid name, positive dimensions, and a
    ///                pixels vector of exactly `width * height * 4` bytes (RGBA).
    /// @param options Packing configuration.
    /// @return A PackResult whose atlases vector is non-empty on success; empty on failure.
    PackResult PackToMemory(std::vector<ImageInput> images, PackOptions const& options);

    /// @brief Pack images into one or more texture atlases, or a single flipbook atlas.
    ///
    /// Loads images from disk, calls PackToMemory, then writes the resulting atlas images
    /// and JSON descriptor to `options.outputPath`.
    ///
    /// When `options.packType == PackType::Atlas`:
    ///   Images are sorted into the smallest power-of-two atlas that achieves the best area
    ///   utilisation. Multiple atlases are produced if needed. The JSON descriptor lists every
    ///   atlas and the rect of each packed image within it.
    ///
    /// When `options.packType == PackType::Flipbook`:
    ///   Images are sorted alphabetically by filename (so frames can be named 001.png, 002.png,
    ///   etc.) and bin-packed into a single atlas. The JSON descriptor uses the new flipbook
    ///   format: a `frames` array with per-frame rects and pivots, and a `clips` array with a
    ///   single auto-generated clip covering all frames in order. Returns false if not all images
    ///   fit into one atlas — increase `maxWidth`/`maxHeight` in that case.
    ///
    /// @param images  Images to pack. Passed by value; the caller's list is unmodified.
    /// @param options Packing configuration.
    /// @return True on success. False on fatal errors such as an empty image list, invalid options,
    ///         a load/write failure, or (flipbook only) images that do not fit in a single atlas.
    bool Pack(std::vector<ImageDetails> imageDetails, PackOptions const& options);

    /// Options controlling sprite extraction from a sheet image.
    struct UnpackOptions {
        std::filesystem::path outputPath;       ///< Directory where extracted sprites are written.
        std::string spritePrefix = "sprite";    ///< Filename prefix for extracted sprite files (e.g. "sprite" → sprite_0.png).
        uint8_t alphaThreshold = 0;             ///< Pixels with alpha strictly greater than this are treated as non-transparent. Only used when no background color is active.
        bool forceOverwrite = false;            ///< Overwrite existing sprite files without error.
        bool dryRun = false;                    ///< Detect sprites and report but do not write any files.
        AtlasFormat format = AtlasFormat::PNG;  ///< Output image format for extracted sprites.
        int jpegQuality = 90;                   ///< JPEG encode quality (1–100). Only used when format is AtlasFormat::JPEG.

        /// Explicit background color (RGB). When set, a pixel is considered background when all
        /// three channels are within colorThreshold of this color. Takes precedence over
        /// alphaThreshold and autoDetectBackground.
        std::optional<std::array<uint8_t, 3>> backgroundColor;

        /// When true and backgroundColor is not set, the background color is inferred by sampling
        /// the four corner pixels of the sheet. Falls back to alpha-based detection if the corners
        /// differ by more than colorThreshold on any channel.
        bool autoDetectBackground = false;

        /// Per-channel tolerance used when comparing pixels against the background color (0–255).
        uint8_t colorThreshold = 10;

        int minSpriteWidth = 0;   ///< Minimum sprite width  to keep (0 = no minimum).
        int minSpriteHeight = 0;  ///< Minimum sprite height to keep (0 = no minimum).
        int maxSpriteWidth = 0;   ///< Maximum sprite width  to keep (0 = no maximum).
        int maxSpriteHeight = 0;  ///< Maximum sprite height to keep (0 = no maximum).

        /// When set, pixels identified as background within each extracted sprite's bounding rect
        /// are replaced with this color (RRGGBBAA) in the output image instead of being copied
        /// from the sheet. Pass 0x00000000 to replace background with full transparency.
        std::optional<uint32_t> replaceBackgroundColor;
    };

    /// @brief Extract individual sprites from a sprite sheet by detecting connected non-background regions.
    ///
    /// A pixel is "active" (part of a sprite) based on the active detection mode:
    /// - If backgroundColor is set: pixel is active when any RGB channel differs from the
    ///   background by more than colorThreshold.
    /// - If autoDetectBackground is true: background color is inferred from the four corner pixels
    ///   (falls back to alpha if corners disagree).
    /// - Otherwise: pixel is active when alpha > alphaThreshold.
    ///
    /// Active pixels are grouped via BFS (8-connectivity) into connected components; the bounding
    /// rect of each component is saved as `<spritePrefix>_N.<ext>`.
    ///
    /// @param sheetPath Path to the source sprite sheet image.
    /// @param options   Extraction configuration.
    /// @return True if all detected sprites were written successfully (or dryRun is true).
    ///         Returns false on load failure, invalid options, or any write error.
    bool Unpack(std::filesystem::path const& sheetPath, UnpackOptions const& options);

} // namespace moth_packer
