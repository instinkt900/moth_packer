#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <moth_ui/moth_ui.h>

namespace moth_packer {

    /// Metadata for a single image to be packed.
    struct ImageDetails {
        std::filesystem::path path;
        moth_ui::IntVec2 dimensions;
        int channels = 0;
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

    /// Options controlling atlas generation.
    /// @note `outputPath` and `filename` are required — Pack() will return false if either is empty.
    struct PackOptions {
        std::filesystem::path outputPath;           ///< Directory where atlas PNGs and the JSON descriptor are written.
        std::string filename;                       ///< Base name for output files (no extension).
        bool forceOverwrite = false;                ///< Overwrite existing output files without error.
        bool dryRun = false;                        ///< Run the full packing pipeline but do not write any files.
        int minWidth = 256;                         ///< Minimum atlas width (rounded up to the next power of two).
        int minHeight = 256;                        ///< Minimum atlas height (rounded up to the next power of two).
        int maxWidth = 4096;                        ///< Maximum atlas width (rounded up to the next power of two).
        int maxHeight = 4096;                       ///< Maximum atlas height (rounded up to the next power of two).
        int padding = 0;                            ///< Pixels of padding added around each image on all sides.
        PaddingType paddingType = PaddingType::Color; ///< How the padding border pixels are filled (Color is a no-op; the atlas background handles it).
        uint32_t paddingColor = 0;                  ///< Atlas background color as RRGGBBAA. Applied to the entire atlas before compositing, so it fills padding regions and any unpacked areas.
        bool prettyJson = false;                    ///< Pretty-print the JSON descriptor with 4-space indentation.
        bool absolutePaths = false;                 ///< Write absolute paths in the JSON descriptor instead of paths relative to outputPath.
        AtlasFormat format = AtlasFormat::PNG;      ///< Output image format for atlas files.
        int jpegQuality = 90;                       ///< JPEG encode quality (1–100). Only used when format is AtlasFormat::JPEG.
    };

    /// @brief Pack images into one or more texture atlases.
    ///
    /// Images are sorted into the smallest power-of-two atlas that achieves the
    /// best area utilisation. If all images do not fit in a single atlas, multiple
    /// atlases are produced. Each atlas is written as a PNG alongside a shared JSON
    /// descriptor listing every atlas and the rect of each packed image within it.
    ///
    /// @param images  Images to pack. Passed by value; the caller's list is unmodified.
    /// @param options Packing configuration.
    /// @return True when packing completes and (when not a dry run) output files are written
    ///         successfully. Oversized images are skipped with a warning but do not cause failure.
    ///         Returns false only on fatal errors such as an empty image list, invalid PackOptions,
    ///         an image that fails to load during compositing, or a file-write failure.
    bool Pack(std::vector<ImageDetails> images, PackOptions const& options);

    /// @deprecated Use Pack(images, PackOptions) instead. This overload will be removed in a future version.
    inline bool Pack(std::vector<ImageDetails> images,
                     std::filesystem::path outputPath,
                     std::string filename,
                     bool forceOverwrite = false,
                     bool dryRun = false,
                     int minWidth = 256,
                     int minHeight = 256,
                     int maxWidth = 4096,
                     int maxHeight = 4096) {
        PackOptions opts;
        opts.outputPath    = std::move(outputPath);
        opts.filename      = std::move(filename);
        opts.forceOverwrite = forceOverwrite;
        opts.dryRun        = dryRun;
        opts.minWidth      = minWidth;
        opts.minHeight     = minHeight;
        opts.maxWidth      = maxWidth;
        opts.maxHeight     = maxHeight;
        return Pack(std::move(images), opts);
    }

    /// Options controlling sprite extraction from a sheet image.
    struct UnpackOptions {
        std::filesystem::path outputPath;       ///< Directory where extracted sprites are written.
        std::string spritePrefix = "sprite";    ///< Filename prefix for extracted sprite files (e.g. "sprite" → sprite_0.png).
        uint8_t alphaThreshold = 0;             ///< Pixels with alpha strictly greater than this are treated as non-transparent. Only used when no background colour is active.
        bool forceOverwrite = false;            ///< Overwrite existing sprite files without error.
        bool dryRun = false;                    ///< Detect sprites and report but do not write any files.
        AtlasFormat format = AtlasFormat::PNG;  ///< Output image format for extracted sprites.
        int jpegQuality = 90;                   ///< JPEG encode quality (1–100). Only used when format is AtlasFormat::JPEG.

        /// Explicit background colour (RGB). When set, a pixel is considered background when all
        /// three channels are within colourThreshold of this colour. Takes precedence over
        /// alphaThreshold and autoDetectBackground.
        std::optional<std::array<uint8_t, 3>> backgroundColour;

        /// When true and backgroundColour is not set, the background colour is inferred by sampling
        /// the four corner pixels of the sheet. Falls back to alpha-based detection if the corners
        /// differ by more than colourThreshold on any channel.
        bool autoDetectBackground = false;

        /// Per-channel tolerance used when comparing pixels against the background colour (0–255).
        uint8_t colourThreshold = 10;

        int minSpriteWidth = 0;   ///< Minimum sprite width  to keep (0 = no minimum).
        int minSpriteHeight = 0;  ///< Minimum sprite height to keep (0 = no minimum).
        int maxSpriteWidth = 0;   ///< Maximum sprite width  to keep (0 = no maximum).
        int maxSpriteHeight = 0;  ///< Maximum sprite height to keep (0 = no maximum).
    };

    /// @brief Extract individual sprites from a sprite sheet by detecting connected non-background regions.
    ///
    /// A pixel is "active" (part of a sprite) based on the active detection mode:
    /// - If backgroundColour is set: pixel is active when any RGB channel differs from the
    ///   background by more than colourThreshold.
    /// - If autoDetectBackground is true: background colour is inferred from the four corner pixels
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

    /// Loop behavior for a flipbook clip.
    enum class LoopType {
        Loop,   ///< Jump back to Start and keep playing indefinitely.
        Stop,   ///< Freeze on the End frame and fire EventFlipbookStopped.
        Reset,  ///< Rewind to Start, freeze, and fire EventFlipbookStopped.
    };

    /// Options controlling flipbook sheet generation.
    struct FlipbookOptions {
        std::filesystem::path outputPath;           ///< Directory where the atlas and JSON descriptor are written.
        std::string filename;                       ///< Base name for output files (no extension).
        bool forceOverwrite = false;                ///< Overwrite existing output files without error.
        bool dryRun = false;                        ///< Run the full pipeline but do not write any files.
        bool prettyJson = false;                    ///< Pretty-print the JSON descriptor with 4-space indentation.
        bool absolutePaths = false;                 ///< Write absolute paths in the JSON descriptor instead of paths relative to outputPath.
        AtlasFormat format = AtlasFormat::PNG;      ///< Output image format for the atlas.
        int jpegQuality = 90;                       ///< JPEG encode quality (1–100). Only used when format is AtlasFormat::JPEG.
        int fps = 12;                               ///< Frames per second for the default clip.
        LoopType loop = LoopType::Loop;             ///< Loop behavior for the default clip.
        int frameWidth = 0;                         ///< Fixed frame width in pixels. 0 = derive from the largest input image.
        int frameHeight = 0;                        ///< Fixed frame height in pixels. 0 = derive from the largest input image.
        bool strict = false;                        ///< If true, oversized frames and atlas size violations cause errors instead of warnings.
        int maxAtlasWidth = 0;                      ///< Maximum atlas width in pixels. 0 = no limit.
        int maxAtlasHeight = 0;                     ///< Maximum atlas height in pixels. 0 = no limit.
    };

    /// @brief Pack images into a uniform-grid flipbook sheet and write a JSON descriptor.
    ///
    /// Images are sorted alphabetically by filename and placed in a roughly-square grid,
    /// each frame centred in a cell of the maximum input image dimensions. A single default
    /// clip covering all frames is written to the descriptor.
    ///
    /// @param images  Images to pack. Passed by value; the caller's list is unmodified.
    /// @param options Flipbook configuration.
    /// @return True when packing completes and (when not a dry run) output files are written
    ///         successfully. Returns false on fatal errors.
    bool Flipbook(std::vector<ImageDetails> images, FlipbookOptions const& options);

} // namespace moth_packer
