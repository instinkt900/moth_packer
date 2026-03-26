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

    /// Strategy used to fill the padding border around each packed image.
    enum class PaddingType {
        Color,  ///< Fill with a solid colour (see PackOptions::paddingColor).
        Extend, ///< Repeat the nearest edge pixel outward.
        Mirror, ///< Reflect pixels across each edge.
        Wrap,   ///< Tile pixels from the opposite edge.
    };

    /// @brief Collect images from a text file containing one image path per line.
    /// @param inputList Path to the text file.
    /// @param dstList   List to append discovered images to. Duplicates are skipped.
    /// @return True if at least one image was added.
    bool CollectImagesFromFile(std::filesystem::path const& inputList, std::vector<ImageDetails>& dstList);

    /// @brief Collect images whose paths match a glob pattern (e.g. "assets/**\/*.png").
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

    /// Options controlling atlas generation. All fields have sensible defaults.
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
        PaddingType paddingType = PaddingType::Color; ///< How the padding border is filled.
        uint32_t paddingColor = 0;                  ///< Padding colour as RRGGBBAA when paddingType is Color.
        bool prettyJson = false;                    ///< Pretty-print the JSON descriptor with 4-space indentation.
        bool absolutePaths = false;                 ///< Write absolute paths in the JSON descriptor instead of paths relative to outputPath.
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
    /// @return True if all images were packed and (when not a dry run) all files written successfully.
    bool Pack(std::vector<ImageDetails> images, PackOptions const& options);

} // namespace moth_packer
