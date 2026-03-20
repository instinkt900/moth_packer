#include "packer.h"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    CLI::App app{"moth_packer — texture atlas packer for moth_ui layouts"};
    argv = app.ensure_utf8(argv);

    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    int minWidth = 256;
    int minHeight = 256;
    int maxWidth = 1024;
    int maxHeight = 1024;

    app.add_option("input", inputPath, "Path to directory containing .moth layout files")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_option("output", outputPath, "Path to directory where packed atlases will be written")
        ->required();

    app.add_option("--min-width", minWidth, "Min final pack width")
        ->default_val(256);

    app.add_option("--min-height", minHeight, "Min final pack height")
        ->default_val(256);

    app.add_option("--max-width", maxWidth, "Max final pack width")
        ->default_val(1024);

    app.add_option("--max-height", maxHeight, "Max final pack height")
        ->default_val(1024);

    if (argc == 1) {
        std::cout << app.help();
        return 0;
    }

    CLI11_PARSE(app, argc, argv);

    std::filesystem::create_directories(outputPath);

    Pack(inputPath, outputPath, minWidth, minHeight, maxWidth, maxHeight);

    return 0;
}
