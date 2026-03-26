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

    bool Pack(std::vector<ImageDetails> images,
              std::filesystem::path const& outputPath,
              std::string const& filename,
              bool forceOverwrite,
              bool dryRun,
              int minWidth,
              int minHeight,
              int maxWidth,
              int maxHeight,
              int padding,
              PaddingType paddingType,
              uint32_t paddingColor);

} // namespace moth_packer
