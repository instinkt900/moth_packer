#include "packer.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_pattern("%v");

    CLI::App app{ fmt::format("moth_packer {} — texture atlas packer for moth_ui layouts", MOTH_PACKER_VERSION) };
    app.set_version_flag("-v,--version", MOTH_PACKER_VERSION);
    argv = app.ensure_utf8(argv);

    std::string outputName;
    app.add_option("output", outputName, "Name of the pack to output (no extension).")
        ->required();

    std::filesystem::path inputTxt;
    std::filesystem::path inputDir;
    std::filesystem::path inputLayout;
    std::filesystem::path inputLayoutDir;

    auto* group = app.add_option_group("input source");
    group->require_option(1);

    auto* optionFile = group->add_option("-i,--file", inputTxt, "Input is given as a list of files in a txt file.")
                           ->check(CLI::ExistingFile);
    auto* optionDir = group->add_option("-d,--dir", inputDir, "Input is all files in a directory.")
                          ->check(CLI::ExistingDirectory);
    auto* optionLayout = group->add_option("-l,--layout", inputLayout, "Input is a single moth_ui layout file.")
                             ->check(CLI::ExistingFile);
    auto* optionLayoutDir = group->add_option("-x,--layout-dir", inputLayoutDir, "Input is a directory of moth_ui layout files.")
                                ->check(CLI::ExistingDirectory);

    bool recursiveInput = false;
    app.add_flag("-r,--recursive", recursiveInput, "Recurse into subdirectories")
        ->default_val(false);

    bool forceOverwrite = false;
    app.add_flag("-f,--force", forceOverwrite, "Force overwritting of output.")
        ->default_val(false);

    std::filesystem::path outputDir;
    app.add_option("-o,--out", outputDir, "Path to directory where packed atlases will be written")
        ->default_val(".");

    std::pair<int, int> minDimensions{ 256, 256 };
    std::pair<int, int> maxDimensions{ 4096, 4096 };

    app.add_option("-n,--min", minDimensions, fmt::format("Min atlas dimensions w,h (default: {},{})", minDimensions.first, minDimensions.second))
        ->delimiter(',');

    app.add_option("-m,--max", maxDimensions, fmt::format("Max atlas dimensions w,h (default: {},{})", maxDimensions.first, maxDimensions.second))
        ->delimiter(',');

    bool verboseMode = false;
    app.add_flag("--verbose", verboseMode, "Verbose mode. All output not just errors and warnings.")
        ->default_val(false);

    bool silentMode = false;
    app.add_flag("--silent", silentMode, "Silent mode. No output.")
        ->default_val(false);

    bool dryRun = false;
    app.add_flag("--dry-run", dryRun, "Dry run. No files written.")
        ->default_val(false);

    if (argc == 1) {
        std::cout << app.help();
        return 0;
    }

    CLI11_PARSE(app, argc, argv);

    if (verboseMode) {
        spdlog::set_level(spdlog::level::trace);
    } else if (silentMode) {
        spdlog::set_level(spdlog::level::off);
    } else {
        spdlog::set_level(spdlog::level::warn);
    }

    std::vector<ImageDetails> images;
    if (optionFile->count() != 0) {
        CollectImagesFromFile(inputTxt, images);
    } else if (optionDir->count() != 0) {
        CollectImagesFromDir(inputDir, recursiveInput, images);
    } else if (optionLayout->count() != 0) {
        CollectImagesFromLayout(inputLayout, images);
    } else if (optionLayoutDir->count() != 0) {
        CollectImagesFromLayoutsDir(inputLayoutDir, recursiveInput, images);
    } else {
        spdlog::error("Unknown input source!");
        return 1;
    }

    if (images.empty()) {
        spdlog::error("No images to pack!");
        return 1;
    }

    std::filesystem::create_directories(outputDir);

    bool result = Pack(images, outputDir, outputName, forceOverwrite, dryRun, minDimensions.first, minDimensions.second, maxDimensions.first, maxDimensions.second);

    return result ? 0 : 1;
}
