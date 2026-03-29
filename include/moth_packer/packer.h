#pragma once

#include <filesystem>
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
        uint8_t alphaThreshold = 0;             ///< Pixels with alpha strictly greater than this are treated as non-transparent.
        bool forceOverwrite = false;            ///< Overwrite existing sprite files without error.
        bool dryRun = false;                    ///< Detect sprites and report but do not write any files.
        AtlasFormat format = AtlasFormat::PNG;  ///< Output image format for extracted sprites.
        int jpegQuality = 90;                   ///< JPEG encode quality (1–100). Only used when format is AtlasFormat::JPEG.
    };

    /// @brief Extract individual sprites from a sprite sheet by detecting connected non-transparent regions.
    ///
    /// Scans the sheet for pixels with alpha > UnpackOptions::alphaThreshold, groups connected pixels
    /// (8-connectivity) into components, and saves the bounding rect of each component as a
    /// separate image file named `<spritePrefix>_N.<ext>`.
    ///
    /// @param sheetPath Path to the source sprite sheet image.
    /// @param options   Extraction configuration.
    /// @return True if all detected sprites were written successfully (or dryRun is true).
    ///         Returns false on load failure, invalid options, or any write error.
    bool Unpack(std::filesystem::path const& sheetPath, UnpackOptions const& options);

} // namespace moth_packer
