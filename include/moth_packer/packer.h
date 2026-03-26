#pragma once

#include <filesystem>
#include <moth_ui/moth_ui.h>

namespace moth_packer {

    struct ImageDetails {
        std::filesystem::path path;
        moth_ui::IntVec2 dimensions;
        int channels = 0;
    };

    enum class PaddingType {
        Color,
        Extend,
        Mirror,
        Wrap,
    };

    bool CollectImagesFromFile(std::filesystem::path const& inputList, std::vector<ImageDetails>& dstList);
    bool CollectImagesFromGlob(std::string const& pattern, std::vector<ImageDetails>& dstList);
    bool CollectImagesFromDir(std::filesystem::path const& inputPath,
                              bool const& recursive,
                              std::vector<ImageDetails>& dstList);
    bool CollectImagesFromLayout(std::filesystem::path const& inputLayout,
                                 std::vector<ImageDetails>& dstList);
    bool CollectImagesFromLayoutsDir(std::filesystem::path const& inputPath,
                                     bool recursive,
                                     std::vector<ImageDetails>& dstList);

    struct PackOptions {
        std::filesystem::path outputPath;
        std::string filename;
        bool forceOverwrite = false;
        bool dryRun = false;
        int minWidth = 256;
        int minHeight = 256;
        int maxWidth = 4096;
        int maxHeight = 4096;
        int padding = 0;
        PaddingType paddingType = PaddingType::Color;
        uint32_t paddingColor = 0;
        bool prettyJson = false;
        bool absolutePaths = false;
    };

    bool Pack(std::vector<ImageDetails> images, PackOptions const& options);

} // namespace moth_packer
