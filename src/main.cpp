#include "moth_packer/packer.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

enum class Mode {
    Pack,
    Unpack,
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
    bool minDimExplicit = false;
    bool maxDimExplicit = false;
    int padding = 0;
    moth_packer::PaddingType paddingType = moth_packer::PaddingType::Color;
    uint32_t paddingColor = 0;

    int alphaThreshold = 0;
    bool autoDetectBackground = false;
    std::string backgroundColor;        // hex RRGGBB, empty = not set
    int colorThreshold = 10;
    std::string replaceBackgroundColor; // hex RRGGBBAA, empty = not set
    std::pair<int, int> spriteSize{ 0, 0 };

    bool prettyJson = false;
    bool absolutePaths = false;

    moth_packer::AtlasFormat atlasFormat = moth_packer::AtlasFormat::PNG;
    int jpegQuality = 90;

    bool dryRun = false;

    moth_packer::PackType packType = moth_packer::PackType::Atlas;
    int fps = 12;
    moth_packer::LoopType loop = moth_packer::LoopType::Loop;
    std::string clipName = "default";

    bool asFlipbook = false;
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
    opts.outputPath          = args.outputDir;
    opts.alphaThreshold      = static_cast<uint8_t>(args.alphaThreshold);
    opts.forceOverwrite      = args.forceOverwrite;
    opts.dryRun              = args.dryRun;
    opts.format              = args.atlasFormat;
    opts.jpegQuality         = args.jpegQuality;
    opts.autoDetectBackground = args.autoDetectBackground;
    opts.colorThreshold     = static_cast<uint8_t>(args.colorThreshold);
    if (args.minDimExplicit) {
        opts.minSpriteWidth  = args.minDimensions.first;
        opts.minSpriteHeight = args.minDimensions.second;
    }
    if (args.maxDimExplicit) {
        opts.maxSpriteWidth  = args.maxDimensions.first;
        opts.maxSpriteHeight = args.maxDimensions.second;
    }
    opts.fixedSpriteWidth  = args.spriteSize.first;
    opts.fixedSpriteHeight = args.spriteSize.second;
    if (!args.backgroundColor.empty()) {
        if (args.backgroundColor.size() != 6) {
            spdlog::error("--bg-color must be a 6-digit hex value (e.g. FF00FF)");
            return 1;
        }
        try {
            unsigned long const rgb = std::stoul(args.backgroundColor, nullptr, 16);
            opts.backgroundColor = std::array<uint8_t, 3>{
                static_cast<uint8_t>((rgb >> 16) & 0xFF),
                static_cast<uint8_t>((rgb >>  8) & 0xFF),
                static_cast<uint8_t>( rgb        & 0xFF),
            };
        } catch (std::exception const&) {
            spdlog::error("--bg-color value '{}' is not a valid hex value", args.backgroundColor);
            return 1;
        }
    }
    if (!args.replaceBackgroundColor.empty()) {
        if (args.replaceBackgroundColor.size() != 8) {
            spdlog::error("--replace-bg-color must be an 8-digit hex value (e.g. FF00FF00)");
            return 1;
        }
        try {
            opts.replaceBackgroundColor = static_cast<uint32_t>(
                std::stoul(args.replaceBackgroundColor, nullptr, 16));
        } catch (std::exception const&) {
            spdlog::error("--replace-bg-color value '{}' is not a valid hex value", args.replaceBackgroundColor);
            return 1;
        }
    }
    opts.outputFlipbook = args.asFlipbook;
    if (args.asFlipbook) {
        opts.fps        = args.fps;
        opts.loop       = args.loop;
        opts.clipName   = args.clipName;
        opts.prettyJson = args.prettyJson;
        opts.absolutePaths = args.absolutePaths;
    }
    return moth_packer::Unpack(args.path, opts) ? 0 : 1;
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
    opts.filename      = std::filesystem::path(args.path).filename().string();
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
    opts.packType      = args.packType;
    opts.fps           = args.fps;
    opts.loop          = args.loop;
    opts.clipName      = args.clipName;
    return moth_packer::Pack(std::move(images), opts) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    spdlog::set_pattern("%v");

    CLI::App app{ fmt::format("moth_packer {} — texture atlas packer for moth_ui layouts",
                              MOTH_PACKER_VERSION) };
    app.set_version_flag("-v,--version", MOTH_PACKER_VERSION);
    argv = app.ensure_utf8(argv);

    Args args;

    // --- General ---
    app.add_option("path", args.path,
                   "Output name for pack mode (no extension), or input sheet path for unpack mode.");

    app.add_option("--mode", args.mode, "Operating mode: pack (default), unpack.")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, Mode>{ { "pack", Mode::Pack },
                                         { "unpack", Mode::Unpack } }));

    app.add_option("-o,--out", args.outputDir, "Path to directory where output files will be written.")
        ->default_val(".");

    app.add_flag("-f,--force", args.forceOverwrite, "Force overwriting of existing output files.")
        ->default_val(false);

    app.add_flag("--dry-run", args.dryRun, "Run without writing any files.")
        ->default_val(false);

    bool verboseMode = false;
    auto* flagVerbose = app.add_flag("--verbose", verboseMode, "Enable verbose output (info-level messages).")
        ->default_val(false);

    bool silentMode = false;
    app.add_flag("--silent", silentMode, "Suppress all output including warnings.")
        ->default_val(false)
        ->excludes(flagVerbose);

    // --- Input source (pack mode) ---
    auto* inputGroup = app.add_option_group("input source (required for pack mode)");
    inputGroup->require_option(0, 1);

    auto* optionFile =
        inputGroup->add_option("-i,--file", args.inputTxt, "List of input image paths, one per line.")
            ->check(CLI::ExistingFile);
    auto* optionDir =
        inputGroup->add_option("-d,--dir", args.inputDir, "All images in a directory.")
            ->check(CLI::ExistingDirectory);
    auto* optionLayout =
        inputGroup->add_option("-l,--layout", args.inputLayout, "Images referenced by a moth_ui layout file.")
            ->check(CLI::ExistingFile);
    auto* optionLayoutDir =
        inputGroup->add_option("-x,--layout-dir", args.inputLayoutDir, "Images referenced by all layouts in a directory.")
            ->check(CLI::ExistingDirectory);
    auto* optionGlob =
        inputGroup->add_option("-g,--glob", args.inputGlob, "Images matching a glob pattern (e.g. assets/**/*.png).");

    app.add_flag("-r,--recursive", args.recursiveInput, "Recurse into subdirectories when using --dir or --layout-dir.")
        ->default_val(false);

    // --- Output format (all modes) ---
    auto* outputGroup = app.add_option_group("output format");

    outputGroup->add_option("--format", args.atlasFormat, "Image format for output atlas files (default: png).")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::AtlasFormat>{ { "png", moth_packer::AtlasFormat::PNG },
                                                             { "bmp", moth_packer::AtlasFormat::BMP },
                                                             { "tga", moth_packer::AtlasFormat::TGA },
                                                             { "jpeg", moth_packer::AtlasFormat::JPEG },
                                                             { "jpg", moth_packer::AtlasFormat::JPEG } }));

    outputGroup->add_option("--jpeg-quality", args.jpegQuality,
                            "JPEG encode quality 1-100 (default: 90, only used with --format jpeg).")
        ->check(CLI::Range(1, 100));

    outputGroup->add_flag("--pretty", args.prettyJson, "Pretty-print the output JSON.")
        ->default_val(false);

    outputGroup->add_flag("--absolute-paths", args.absolutePaths, "Write absolute paths in the output JSON.")
        ->default_val(false);

    // --- Dimension bounds (all modes) ---
    auto* optionMinDim =
        app.add_option("--min-dim", args.minDimensions,
                       fmt::format("pack: minimum atlas dimensions WxH (default: {}x{}). "
                                   "unpack: minimum sprite size to keep; smaller sprites are discarded (default: no limit).",
                                   args.minDimensions.first, args.minDimensions.second))
            ->delimiter('x');

    auto* optionMaxDim =
        app.add_option("--max-dim", args.maxDimensions,
                       fmt::format("pack: maximum atlas dimensions WxH (default: {}x{}). "
                                   "unpack: maximum sprite size to keep; larger sprites are discarded (default: no limit).",
                                   args.maxDimensions.first, args.maxDimensions.second))
            ->delimiter('x');

    // --- Pack options ---
    auto* packGroup = app.add_option_group("pack options");

    packGroup->add_option("--pack-type", args.packType,
                          "Output type for pack mode: atlas (default) or flipbook. "
                          "atlas writes a multi-atlas JSON descriptor. "
                          "flipbook writes a single-atlas .flipbook.json with per-frame rects and a clip sequence.")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::PackType>{ { "atlas",    moth_packer::PackType::Atlas },
                                                          { "flipbook", moth_packer::PackType::Flipbook } }));

    packGroup->add_option("-p,--padding", args.padding, "Pixels of padding around each image.")
        ->default_val(0);

    packGroup->add_option("--padding-type", args.paddingType, "Padding fill strategy (default: color).")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::PaddingType>{ { "color", moth_packer::PaddingType::Color },
                                                             { "extend", moth_packer::PaddingType::Extend },
                                                             { "mirror", moth_packer::PaddingType::Mirror },
                                                             { "wrap", moth_packer::PaddingType::Wrap } }));

    packGroup->add_option("--padding-color", args.paddingColor, "Atlas background fill color as RRGGBBAA hex.")
        ->transform([](std::string const& val) -> std::string {
            if (val.size() != 8 || val.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                throw CLI::ValidationError("--padding-color", "must be exactly 8 hex digits");
            }
            return std::to_string(std::stoul(val, nullptr, 16));
        });

    packGroup->add_option("--fps", args.fps,
                          "Frames per second for the auto-generated clip (default: 12, flipbook only).")
        ->check(CLI::Range(1, 1000));

    packGroup->add_option("--loop", args.loop,
                          "Loop behavior for the auto-generated clip (default: loop, flipbook only).")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, moth_packer::LoopType>{ { "loop",  moth_packer::LoopType::Loop },
                                                          { "stop",  moth_packer::LoopType::Stop },
                                                          { "reset", moth_packer::LoopType::Reset } }));

    packGroup->add_option("--clip-name", args.clipName,
                          "Name for the auto-generated clip (default: \"default\", flipbook only).");

    // --- Unpack options ---
    auto* unpackGroup = app.add_option_group("unpack options");

    unpackGroup->add_option("--alpha-threshold", args.alphaThreshold,
                            "Pixels with alpha above this value are treated as opaque (0-255, default: 0). "
                            "Only used when no background color is active.")
        ->check(CLI::Range(0, 255));

    unpackGroup->add_flag("--auto-bg", args.autoDetectBackground,
                          "Auto-detect background color by sampling the four corners of the sheet. "
                          "Falls back to alpha-based detection if the corners disagree.");

    unpackGroup->add_option("--bg-color", args.backgroundColor,
                            "Explicit background color as a 6-digit hex value (e.g. FF00FF). "
                            "Overrides --auto-bg and --alpha-threshold.");

    unpackGroup->add_option("--color-threshold", args.colorThreshold,
                            "Per-channel tolerance when comparing pixels against the background color (0-255, default: 10).")
        ->check(CLI::Range(0, 255));

    unpackGroup->add_option("--replace-bg-color", args.replaceBackgroundColor,
                            "Replace detected background pixels in each extracted sprite with this color, "
                            "given as an 8-digit hex RRGGBBAA value (e.g. 00000000 for full transparency). "
                            "Requires a background detection mode (--bg-color, --auto-bg, or --alpha-threshold).");

    unpackGroup->add_option("--sprite-size", args.spriteSize,
                            "Fixed sprite size WxH for tiling extraction (e.g. 96x96). "
                            "When set, flood-fill detection is skipped and the sheet is broken into "
                            "uniform tiles of this size, extracted row-by-row. "
                            "Partial tiles at the right or bottom edge are skipped.")
        ->delimiter('x');

    unpackGroup->add_flag("--as-flipbook", args.asFlipbook,
                          "Write a .flipbook.json referencing the original sheet instead of extracting "
                          "individual sprite images. The detected frame rects become the flipbook frames. "
                          "Use --fps, --loop, and --clip-name to configure the auto-generated clip.")
        ->default_val(false);

    if (argc == 1) {
        std::cout << app.help();
        return 0;
    }

    CLI11_PARSE(app, argc, argv);

    if (verboseMode) {
        spdlog::set_level(spdlog::level::info);
    } else if (silentMode) {
        spdlog::set_level(spdlog::level::err);
    } else {
        spdlog::set_level(spdlog::level::warn);
    }

    args.minDimExplicit = (optionMinDim->count() != 0);
    args.maxDimExplicit = (optionMaxDim->count() != 0);

    if (optionFile->count() != 0)      { args.inputSource = InputSource::File; }
    else if (optionDir->count() != 0)  { args.inputSource = InputSource::Dir; }
    else if (optionLayout->count() != 0) { args.inputSource = InputSource::Layout; }
    else if (optionLayoutDir->count() != 0) { args.inputSource = InputSource::LayoutDir; }
    else if (optionGlob->count() != 0) { args.inputSource = InputSource::Glob; }

    switch (args.mode) {
    case Mode::Pack:   return RunPack(args);
    case Mode::Unpack: return RunUnpack(args);
    default:
        spdlog::error("Unhandled mode");
        return 1;
    }
}
