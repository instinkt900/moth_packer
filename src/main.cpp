#include "moth_packer/packer.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_pattern("%v");

    CLI::App app{ fmt::format("moth_packer {} — texture atlas packer for moth_ui layouts",
                              MOTH_PACKER_VERSION) };
    app.set_version_flag("-v,--version", MOTH_PACKER_VERSION);
    argv = app.ensure_utf8(argv);

    std::string outputName;
    app.add_option("output", outputName, "Name of the pack to output (no extension).")->required();

    std::filesystem::path inputTxt;
    std::filesystem::path inputDir;
    std::filesystem::path inputLayout;
    std::filesystem::path inputLayoutDir;
    std::string inputGlob;

    auto* group = app.add_option_group("input source");
    group->require_option(1);

    auto* optionFile =
        group->add_option("-i,--file", inputTxt, "Input is given as a list of files in a txt file.")
            ->check(CLI::ExistingFile);
    auto* optionDir = group->add_option("-d,--dir", inputDir, "Input is all files in a directory.")
                          ->check(CLI::ExistingDirectory);
    auto* optionLayout =
        group->add_option("-l,--layout", inputLayout, "Input is a single moth_ui layout file.")
            ->check(CLI::ExistingFile);
    auto* optionLayoutDir =
        group->add_option("-x,--layout-dir", inputLayoutDir, "Input is a directory of moth_ui layout files.")
            ->check(CLI::ExistingDirectory);
    auto* optionGlob =
        group->add_option("-g,--glob", inputGlob, "Input is a glob pattern (e.g. assets/**/*.png).");

    bool recursiveInput = false;
    app.add_flag("-r,--recursive", recursiveInput, "Recurse into subdirectories")->default_val(false);

    bool forceOverwrite = false;
    app.add_flag("-f,--force", forceOverwrite, "Force overwritting of output.")->default_val(false);

    std::filesystem::path outputDir;
    app.add_option("-o,--out", outputDir, "Path to directory where packed atlases will be written")
        ->default_val(".");

    std::pair<int, int> minDimensions{ 256, 256 };
    std::pair<int, int> maxDimensions{ 4096, 4096 };

    app.add_option("-n,--min",
                   minDimensions,
                   fmt::format("Min atlas dimensions w,h (default: {},{})",
                               minDimensions.first,
                               minDimensions.second))
        ->delimiter(',');

    app.add_option("-m,--max",
                   maxDimensions,
                   fmt::format("Max atlas dimensions w,h (default: {},{})",
                               maxDimensions.first,
                               maxDimensions.second))
        ->delimiter(',');

    int padding = 0;
    app.add_option("-p,--padding", padding, "Padding around images.")->default_val(0);

    moth_packer::PaddingType paddingType = moth_packer::PaddingType::Color;
    app.add_option("-t,--padding-type", paddingType, "Padding type")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::PaddingType>{ { "color", moth_packer::PaddingType::Color },
                                                             { "extend", moth_packer::PaddingType::Extend },
                                                             { "mirror", moth_packer::PaddingType::Mirror },
                                                             { "wrap", moth_packer::PaddingType::Wrap } }));

    uint32_t paddingColor = 0;
    app.add_option("-c,--padding-color", paddingColor, "Padding colour as RRGGBBAA hex")
        ->transform([](std::string const& val) -> std::string {
            if (val.size() != 8 || val.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                throw CLI::ValidationError("-c,--padding-color", "must be exactly 8 hex digits");
            }
            return std::to_string(std::stoul(val, nullptr, 16));
        });

    bool verboseMode = false;
    app.add_flag("--verbose", verboseMode, "Verbose mode. All output not just errors and warnings.")
        ->default_val(false);

    bool silentMode = false;
    app.add_flag("--silent", silentMode, "Silent mode. No output.")->default_val(false);

    bool dryRun = false;
    app.add_flag("--dry-run", dryRun, "Dry run. No files written.")->default_val(false);

    bool prettyJson = false;
    app.add_flag("--pretty", prettyJson, "Pretty print the output JSON.")->default_val(false);

    bool absolutePaths = false;
    app.add_flag("--absolute-paths", absolutePaths, "Write absolute paths in the output JSON.")
        ->default_val(false);

    if (argc == 1) {
        std::cout << app.help();
        return 0;
    }

    CLI11_PARSE(app, argc, argv);

    if (verboseMode) {
        spdlog::set_level(spdlog::level::trace);
    } else if (silentMode) {
        // should silent mode silence ALL info? I feel like errors should always be visible.
        spdlog::set_level(spdlog::level::err);
    } else {
        spdlog::set_level(spdlog::level::warn);
    }

    std::vector<moth_packer::ImageDetails> images;
    if (optionFile->count() != 0) {
        moth_packer::CollectImagesFromFile(inputTxt, images);
    } else if (optionDir->count() != 0) {
        moth_packer::CollectImagesFromDir(inputDir, recursiveInput, images);
    } else if (optionLayout->count() != 0) {
        moth_packer::CollectImagesFromLayout(inputLayout, images);
    } else if (optionLayoutDir->count() != 0) {
        moth_packer::CollectImagesFromLayoutsDir(inputLayoutDir, recursiveInput, images);
    } else if (optionGlob->count() != 0) {
        moth_packer::CollectImagesFromGlob(inputGlob, images);
    } else {
        spdlog::error("Unknown input source!");
        return 1;
    }

    if (images.empty()) {
        spdlog::error("No images to pack!");
        return 1;
    }

    std::filesystem::create_directories(outputDir);

    moth_packer::PackOptions packOptions;
    packOptions.outputPath = outputDir;
    packOptions.filename = outputName;
    packOptions.forceOverwrite = forceOverwrite;
    packOptions.dryRun = dryRun;
    packOptions.minWidth = minDimensions.first;
    packOptions.minHeight = minDimensions.second;
    packOptions.maxWidth = maxDimensions.first;
    packOptions.maxHeight = maxDimensions.second;
    packOptions.padding = padding;
    packOptions.paddingType = paddingType;
    packOptions.paddingColor = paddingColor;
    packOptions.prettyJson = prettyJson;
    packOptions.absolutePaths = absolutePaths;

    bool result = moth_packer::Pack(images, packOptions);

    return result ? 0 : 1;
}
