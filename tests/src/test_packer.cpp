#include <catch2/catch_all.hpp>

#include "packer.h"

#include "stb_image_write.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <filesystem>
#include <fstream>

struct SilenceSpdlog : Catch::EventListenerBase {
    using EventListenerBase::EventListenerBase;
    void testRunStarting(Catch::TestRunInfo const& /*info*/) override {
        spdlog::set_level(spdlog::level::off);
    }
};
CATCH_REGISTER_LISTENER(SilenceSpdlog)

namespace {
    // RAII temporary directory — created on construction, recursively deleted on destruction
    struct TempDir {
        std::filesystem::path path;

        TempDir() {
            static std::atomic<int> s_counter{ 0 };
            auto const name = "moth_packer_tests_" + std::to_string(++s_counter);
            path = std::filesystem::temp_directory_path() / name;
            std::filesystem::create_directories(path);
        }

        ~TempDir() {
            std::filesystem::remove_all(path);
        }

        TempDir(TempDir const&) = delete;
        TempDir& operator=(TempDir const&) = delete;
    };

    // Write a solid-colour RGBA PNG of the given dimensions and return an ImageDetails for it
    ImageDetails MakeTestImage(std::filesystem::path const& dir, std::string const& name, int width, int height) {
        auto const filePath = dir / name;
        int const channels = 4;
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * channels, 255);
        stbi_write_png(filePath.string().c_str(), width, height, channels, pixels.data(), width * channels);

        ImageDetails details;
        details.path = filePath;
        details.dimensions = { width, height };
        details.channels = channels;
        return details;
    }
}

TEST_CASE("Pack returns false for empty image list", "[pack]") {
    TempDir out;
    CHECK_FALSE(Pack({}, out.path, "test", false, false, 256, 256, 4096, 4096));
}

TEST_CASE("Pack returns false when output path does not exist", "[pack]") {
    TempDir src;
    auto images = { MakeTestImage(src.path, "a.png", 16, 16) };
    CHECK_FALSE(Pack({ images }, std::filesystem::path("/nonexistent/path"), "test", false, false, 256, 256, 4096, 4096));
}

TEST_CASE("Pack returns false when output JSON already exists and forceOverwrite is false", "[pack]") {
    TempDir src;
    TempDir out;
    auto images = { MakeTestImage(src.path, "a.png", 16, 16) };

    // pre-create the descriptor file
    std::ofstream(out.path / "test.json").close();

    CHECK_FALSE(Pack({ images }, out.path, "test", false, false, 256, 256, 4096, 4096));
}

TEST_CASE("Pack succeeds when output JSON already exists and forceOverwrite is true", "[pack]") {
    TempDir src;
    TempDir out;
    auto images = { MakeTestImage(src.path, "a.png", 16, 16) };

    std::ofstream(out.path / "test.json").close();

    CHECK(Pack({ images }, out.path, "test", true, false, 256, 256, 4096, 4096));
}

TEST_CASE("Pack dry run returns true and writes no files", "[pack]") {
    TempDir src;
    TempDir out;
    auto images = { MakeTestImage(src.path, "a.png", 16, 16) };

    CHECK(Pack({ images }, out.path, "test", false, true, 256, 256, 4096, 4096));
    CHECK_FALSE(std::filesystem::exists(out.path / "test.json"));
    CHECK_FALSE(std::filesystem::exists(out.path / "test_0.png"));
}

TEST_CASE("Pack writes JSON descriptor and atlas PNG on success", "[pack]") {
    TempDir src;
    TempDir out;
    auto images = { MakeTestImage(src.path, "a.png", 16, 16) };

    CHECK(Pack({ images }, out.path, "test", false, false, 256, 256, 4096, 4096));
    CHECK(std::filesystem::exists(out.path / "test.json"));
    CHECK(std::filesystem::exists(out.path / "test_0.png"));
}

TEST_CASE("Pack skips images that exceed max atlas dimensions", "[pack]") {
    TempDir src;
    TempDir out;

    // one image that fits, one that doesn't
    std::vector<ImageDetails> images = {
        MakeTestImage(src.path, "small.png", 16, 16),
        MakeTestImage(src.path, "big.png", 64, 64),
    };

    // max is 32x32, so big.png should be skipped but small.png still packs
    CHECK(Pack(images, out.path, "test", false, false, 16, 16, 32, 32));
    CHECK(std::filesystem::exists(out.path / "test.json"));
}

TEST_CASE("Pack produces multiple atlases when images do not fit in one", "[pack]") {
    TempDir src;
    TempDir out;

    // each image is 32x32, atlas max is 32x32 — each image needs its own atlas
    std::vector<ImageDetails> images = {
        MakeTestImage(src.path, "a.png", 32, 32),
        MakeTestImage(src.path, "b.png", 32, 32),
        MakeTestImage(src.path, "c.png", 32, 32),
    };

    CHECK(Pack(images, out.path, "test", false, false, 32, 32, 32, 32));
    CHECK(std::filesystem::exists(out.path / "test_0.png"));
    CHECK(std::filesystem::exists(out.path / "test_1.png"));
    CHECK(std::filesystem::exists(out.path / "test_2.png"));
}

TEST_CASE("Pack JSON descriptor contains correct relative paths", "[pack]") {
    // root/
    //   out/      <- output directory
    TempDir root;
    auto const outPath = root.path / "out";
    std::filesystem::create_directories(outPath);

    ImageDetails image;

    SECTION("image in same directory as output") {
        image = MakeTestImage(outPath, "a.png", 16, 16);
    }

    SECTION("image in subdirectory of output") {
        std::filesystem::create_directories(outPath / "sub");
        image = MakeTestImage(outPath / "sub", "a.png", 16, 16);
    }

    SECTION("image in parent directory of output") {
        image = MakeTestImage(root.path, "a.png", 16, 16);
    }

    REQUIRE(Pack({ image }, outPath, "test", false, false, 256, 256, 4096, 4096));

    std::ifstream f(outPath / "test.json");
    auto const json = nlohmann::json::parse(f);

    auto const& atlases = json["atlases"];
    REQUIRE(atlases.size() == 1);

    CHECK(atlases[0]["atlas"] == "test_0.png");

    auto const& images = atlases[0]["images"];
    REQUIRE(images.size() == 1);

    // image path should be relative to the output directory and resolve back to the source file
    auto const relPath = std::filesystem::path(images[0]["path"].get<std::string>());
    auto const resolvedPath = std::filesystem::weakly_canonical(outPath / relPath);
    CHECK(resolvedPath == std::filesystem::weakly_canonical(image.path));
}
