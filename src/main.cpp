#include "moth_packer/packer.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

enum class Mode {
    Pack,
    Unpack,
    Flipbook,
};

enum class InputSource {
    None,
    File,
    Dir,
    Layout,
    LayoutDir,
    Glob,
};

struct Args {
    std::string path;
    Mode mode = Mode::Pack;

    InputSource inputSource = InputSource::None;
    std::filesystem::path inputTxt;
    std::filesystem::path inputDir;
    std::filesystem::path inputLayout;
    std::filesystem::path inputLayoutDir;
    std::string inputGlob;
    bool recursiveInput = false;

    bool forceOverwrite = false;
    std::filesystem::path outputDir = ".";

    std::pair<int, int> minDimensions{ 256, 256 };
    std::pair<int, int> maxDimensions{ 4096, 4096 };
    int padding = 0;
    moth_packer::PaddingType paddingType = moth_packer::PaddingType::Color;
    uint32_t paddingColor = 0;

    int alphaThreshold = 0;

    bool prettyJson = false;
    bool absolutePaths = false;

    moth_packer::AtlasFormat atlasFormat = moth_packer::AtlasFormat::PNG;
    int jpegQuality = 90;

    bool dryRun = false;

    int fps = 12;
    moth_packer::LoopType loop = moth_packer::LoopType::Loop;
};

static bool CollectImages(Args const& args, std::vector<moth_packer::ImageDetails>& images) {
    switch (args.inputSource) {
    case InputSource::File:      return moth_packer::CollectImagesFromFile(args.inputTxt, images);
    case InputSource::Dir:       return moth_packer::CollectImagesFromDir(args.inputDir, args.recursiveInput, images);
    case InputSource::Layout:    return moth_packer::CollectImagesFromLayout(args.inputLayout, images);
    case InputSource::LayoutDir: return moth_packer::CollectImagesFromLayoutsDir(args.inputLayoutDir, args.recursiveInput, images);
    case InputSource::Glob:      return moth_packer::CollectImagesFromGlob(args.inputGlob, images);
    case InputSource::None:      return false;
    }
    return false;
}

int RunUnpack(Args const& args) {
    if (args.path.empty()) {
        spdlog::error("sheet path is required for unpack mode");
        return 1;
    }
    moth_packer::UnpackOptions opts;
    opts.outputPath     = args.outputDir;
    opts.alphaThreshold = static_cast<uint8_t>(args.alphaThreshold);
    opts.forceOverwrite = args.forceOverwrite;
    opts.dryRun         = args.dryRun;
    opts.format         = args.atlasFormat;
    opts.jpegQuality    = args.jpegQuality;
    return moth_packer::Unpack(args.path, opts) ? 0 : 1;
}

int RunFlipbook(Args const& args) {
    if (args.path.empty()) {
        spdlog::error("output name is required for flipbook mode");
        return 1;
    }
    if (args.inputSource == InputSource::None) {
        spdlog::error("an input source is required for flipbook mode");
        return 1;
    }
    std::vector<moth_packer::ImageDetails> images;
    CollectImages(args, images);
    if (images.empty()) {
        spdlog::error("No images found!");
        return 1;
    }
    moth_packer::FlipbookOptions opts;
    opts.outputPath    = args.outputDir;
    opts.filename      = args.path;
    opts.forceOverwrite = args.forceOverwrite;
    opts.dryRun        = args.dryRun;
    opts.prettyJson    = args.prettyJson;
    opts.absolutePaths = args.absolutePaths;
    opts.format        = args.atlasFormat;
    opts.jpegQuality   = args.jpegQuality;
    opts.fps           = args.fps;
    opts.loop          = args.loop;
    return moth_packer::Flipbook(std::move(images), opts) ? 0 : 1;
}

int RunPack(Args const& args) {
    if (args.path.empty()) {
        spdlog::error("output name is required for pack mode");
        return 1;
    }
    if (args.inputSource == InputSource::None) {
        spdlog::error("an input source is required for pack mode");
        return 1;
    }
    std::vector<moth_packer::ImageDetails> images;
    CollectImages(args, images);
    if (images.empty()) {
        spdlog::error("No images found!");
        return 1;
    }
    moth_packer::PackOptions opts;
    opts.outputPath    = args.outputDir;
    opts.filename      = args.path;
    opts.forceOverwrite = args.forceOverwrite;
    opts.dryRun        = args.dryRun;
    opts.minWidth      = args.minDimensions.first;
    opts.minHeight     = args.minDimensions.second;
    opts.maxWidth      = args.maxDimensions.first;
    opts.maxHeight     = args.maxDimensions.second;
    opts.padding       = args.padding;
    opts.paddingType   = args.paddingType;
    opts.paddingColor  = args.paddingColor;
    opts.prettyJson    = args.prettyJson;
    opts.absolutePaths = args.absolutePaths;
    opts.format        = args.atlasFormat;
    opts.jpegQuality   = args.jpegQuality;
    return moth_packer::Pack(std::move(images), opts) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    spdlog::set_pattern("%v");

    CLI::App app{ fmt::format("moth_packer {} — texture atlas packer for moth_ui layouts",
                              MOTH_PACKER_VERSION) };
    app.set_version_flag("-v,--version", MOTH_PACKER_VERSION);
    argv = app.ensure_utf8(argv);

    Args args;

    app.add_option("path", args.path,
                   "Output name for pack/flipbook modes (no extension), or input sheet path for unpack mode.");

    app.add_option("--mode", args.mode, "Operating mode: pack (default), unpack, flipbook.")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, Mode>{ { "pack", Mode::Pack },
                                         { "unpack", Mode::Unpack },
                                         { "flipbook", Mode::Flipbook } }));

    auto* group = app.add_option_group("input source (required for pack and flipbook modes)");
    group->require_option(0, 1);

    auto* optionFile =
        group->add_option("-i,--file", args.inputTxt, "Input is given as a list of files in a txt file.")
            ->check(CLI::ExistingFile);
    auto* optionDir = group->add_option("-d,--dir", args.inputDir, "Input is all files in a directory.")
                          ->check(CLI::ExistingDirectory);
    auto* optionLayout =
        group->add_option("-l,--layout", args.inputLayout, "Input is a single moth_ui layout file.")
            ->check(CLI::ExistingFile);
    auto* optionLayoutDir =
        group->add_option("-x,--layout-dir", args.inputLayoutDir, "Input is a directory of moth_ui layout files.")
            ->check(CLI::ExistingDirectory);
    auto* optionGlob =
        group->add_option("-g,--glob", args.inputGlob, "Input is a glob pattern (e.g. assets/**/*.png).");

    app.add_flag("-r,--recursive", args.recursiveInput, "Recurse into subdirectories")->default_val(false);
    app.add_flag("-f,--force", args.forceOverwrite, "Force overwritting of output.")->default_val(false);

    app.add_option("-o,--out", args.outputDir, "Path to directory where output files will be written")
        ->default_val(".");

    app.add_option("-n,--min",
                   args.minDimensions,
                   fmt::format("Min atlas dimensions w,h (default: {},{})",
                               args.minDimensions.first,
                               args.minDimensions.second))
        ->delimiter(',');

    app.add_option("-m,--max",
                   args.maxDimensions,
                   fmt::format("Max atlas dimensions w,h (default: {},{})",
                               args.maxDimensions.first,
                               args.maxDimensions.second))
        ->delimiter(',');

    app.add_option("-p,--padding", args.padding, "Padding around images.")->default_val(0);

    app.add_option("-t,--padding-type", args.paddingType, "Padding type")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::PaddingType>{ { "color", moth_packer::PaddingType::Color },
                                                             { "extend", moth_packer::PaddingType::Extend },
                                                             { "mirror", moth_packer::PaddingType::Mirror },
                                                             { "wrap", moth_packer::PaddingType::Wrap } }));

    app.add_option("-c,--padding-color", args.paddingColor, "Padding color as RRGGBBAA hex")
        ->transform([](std::string const& val) -> std::string {
            if (val.size() != 8 || val.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                throw CLI::ValidationError("-c,--padding-color", "must be exactly 8 hex digits");
            }
            return std::to_string(std::stoul(val, nullptr, 16));
        });

    app.add_option("--alpha-threshold", args.alphaThreshold,
                   "Alpha threshold for sprite detection in unpack mode (0-255, default: 0)")
        ->check(CLI::Range(0, 255));

    bool verboseMode = false;
    app.add_flag("--verbose", verboseMode, "Verbose mode. All output not just errors and warnings.")
        ->default_val(false);

    bool silentMode = false;
    app.add_flag("--silent", silentMode, "Silent mode. No output.")->default_val(false);

    app.add_flag("--dry-run", args.dryRun, "Dry run. No files written.")->default_val(false);
    app.add_flag("--pretty", args.prettyJson, "Pretty print the output JSON.")->default_val(false);
    app.add_flag("--absolute-paths", args.absolutePaths, "Write absolute paths in the output JSON.")
        ->default_val(false);

    app.add_option("--format", args.atlasFormat, "Output image format for atlas files (default: png)")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::AtlasFormat>{ { "png", moth_packer::AtlasFormat::PNG },
                                                             { "bmp", moth_packer::AtlasFormat::BMP },
                                                             { "tga", moth_packer::AtlasFormat::TGA },
                                                             { "jpeg", moth_packer::AtlasFormat::JPEG },
                                                             { "jpg", moth_packer::AtlasFormat::JPEG } }));

    app.add_option("--jpeg-quality", args.jpegQuality, "JPEG encode quality 1-100 (default: 90, only used with --format jpeg)")
        ->check(CLI::Range(1, 100));

    app.add_option("--fps", args.fps, "Frames per second for the default flipbook clip (default: 12, flipbook mode only)")
        ->check(CLI::Range(1, 1000));

    app.add_option("--loop", args.loop, "Loop behaviour for the default flipbook clip (default: loop, flipbook mode only)")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::LoopType>{ { "loop", moth_packer::LoopType::Loop },
                                                          { "stop", moth_packer::LoopType::Stop },
                                                          { "reset", moth_packer::LoopType::Reset } }));

    if (argc == 1) {
        std::cout << app.help();
        return 0;
    }

    CLI11_PARSE(app, argc, argv);

    if (verboseMode) {
        spdlog::set_level(spdlog::level::trace);
    } else if (silentMode) {
        spdlog::set_level(spdlog::level::err);
    } else {
        spdlog::set_level(spdlog::level::warn);
    }

    if (optionFile->count() != 0)      { args.inputSource = InputSource::File; }
    else if (optionDir->count() != 0)  { args.inputSource = InputSource::Dir; }
    else if (optionLayout->count() != 0) { args.inputSource = InputSource::Layout; }
    else if (optionLayoutDir->count() != 0) { args.inputSource = InputSource::LayoutDir; }
    else if (optionGlob->count() != 0) { args.inputSource = InputSource::Glob; }

    std::filesystem::create_directories(args.outputDir);

    switch (args.mode) {
    case Mode::Pack:     return RunPack(args);
    case Mode::Unpack:   return RunUnpack(args);
    case Mode::Flipbook: return RunFlipbook(args);
    }

    return 0;
}
