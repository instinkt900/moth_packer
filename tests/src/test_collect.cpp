#include <catch2/catch_all.hpp>

#include "packer.h"
#include "test_helpers.h"

#include <filesystem>
#include <fstream>

// ---------------------------------------------------------------------------
// CollectImagesFromFile
// ---------------------------------------------------------------------------

TEST_CASE("CollectImagesFromFile returns false for non-existent file", "[collect][file]") {
    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromFile("/nonexistent/list.txt", dst));
}

TEST_CASE("CollectImagesFromFile returns false when no valid images in file", "[collect][file]") {
    TempDir dir;
    auto const listPath = dir.path / "list.txt";

    SECTION("empty file") {
        std::ofstream(listPath).close();
    }

    SECTION("file with only unsupported extensions") {
        std::ofstream f(listPath);
        f << (dir.path / "image.psd").string() << "\n";
        f << (dir.path / "image.gif").string() << "\n";
    }

    SECTION("file with only missing paths") {
        std::ofstream f(listPath);
        f << (dir.path / "missing.png").string() << "\n";
    }

    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromFile(listPath, dst));
    CHECK(dst.empty());
}

TEST_CASE("CollectImagesFromFile collects valid images", "[collect][file]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);

    auto const listPath = dir.path / "list.txt";
    std::ofstream(listPath) << img.path.string() << "\n";

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromFile(listPath, dst));
    REQUIRE(dst.size() == 1);
    CHECK(dst[0].path == img.path);
    CHECK(dst[0].dimensions.x == 16);
    CHECK(dst[0].dimensions.y == 16);
}

TEST_CASE("CollectImagesFromFile skips duplicate entries", "[collect][file]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);

    auto const listPath = dir.path / "list.txt";
    {
        std::ofstream f(listPath);
        f << img.path.string() << "\n";
        f << img.path.string() << "\n"; // duplicate
    }

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromFile(listPath, dst));
    CHECK(dst.size() == 1);
}

// ---------------------------------------------------------------------------
// CollectImagesFromDir
// ---------------------------------------------------------------------------

TEST_CASE("CollectImagesFromDir returns false for non-existent directory", "[collect][dir]") {
    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromDir("/nonexistent/dir", false, dst));
}

TEST_CASE("CollectImagesFromDir returns false when directory has no supported images", "[collect][dir]") {
    TempDir dir;

    SECTION("empty directory") {}

    SECTION("directory with only unsupported files") {
        std::ofstream(dir.path / "doc.txt").close();
        std::ofstream(dir.path / "image.psd").close();
    }

    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromDir(dir.path, false, dst));
    CHECK(dst.empty());
}

TEST_CASE("CollectImagesFromDir collects images from flat directory", "[collect][dir]") {
    TempDir dir;
    MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestImage(dir.path, "b.png", 32, 32);

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromDir(dir.path, false, dst));
    CHECK(dst.size() == 2);
}

TEST_CASE("CollectImagesFromDir non-recursive does not enter subdirectories", "[collect][dir]") {
    TempDir dir;
    MakeTestImage(dir.path, "a.png", 16, 16);
    std::filesystem::create_directories(dir.path / "sub");
    MakeTestImage(dir.path / "sub", "b.png", 16, 16);

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromDir(dir.path, false, dst));
    CHECK(dst.size() == 1);
}

TEST_CASE("CollectImagesFromDir recursive enters subdirectories", "[collect][dir]") {
    TempDir dir;
    MakeTestImage(dir.path, "a.png", 16, 16);
    std::filesystem::create_directories(dir.path / "sub");
    MakeTestImage(dir.path / "sub", "b.png", 16, 16);

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromDir(dir.path, true, dst));
    CHECK(dst.size() == 2);
}

TEST_CASE("CollectImagesFromDir skips images already in destination list", "[collect][dir]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);

    std::vector<ImageDetails> dst = { img }; // pre-populate with the same image
    CHECK_FALSE(CollectImagesFromDir(dir.path, false, dst)); // returns false — nothing new found
    CHECK(dst.size() == 1);
}

// ---------------------------------------------------------------------------
// CollectImagesFromLayout
// ---------------------------------------------------------------------------

TEST_CASE("CollectImagesFromLayout returns false for non-existent layout", "[collect][layout]") {
    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromLayout("/nonexistent/layout.moth_layout", dst));
}

TEST_CASE("CollectImagesFromLayout returns false for layout with no image entities", "[collect][layout]") {
    TempDir dir;
    auto const layoutPath = MakeTestLayout(dir.path, "empty", {});

    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromLayout(layoutPath, dst));
    CHECK(dst.empty());
}

TEST_CASE("CollectImagesFromLayout collects images referenced by layout", "[collect][layout]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    auto const layoutPath = MakeTestLayout(dir.path, "test", { img.path });

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromLayout(layoutPath, dst));
    REQUIRE(dst.size() == 1);
    CHECK(dst[0].path == img.path);
}

TEST_CASE("CollectImagesFromLayout skips duplicate images across entities", "[collect][layout]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    // same image referenced twice in the layout
    auto const layoutPath = MakeTestLayout(dir.path, "test", { img.path, img.path });

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromLayout(layoutPath, dst));
    CHECK(dst.size() == 1);
}

// ---------------------------------------------------------------------------
// CollectImagesFromLayoutsDir
// ---------------------------------------------------------------------------

TEST_CASE("CollectImagesFromLayoutsDir returns false for non-existent directory", "[collect][layouts_dir]") {
    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromLayoutsDir("/nonexistent/dir", false, dst));
}

TEST_CASE("CollectImagesFromLayoutsDir returns false when no images found", "[collect][layouts_dir]") {
    TempDir dir;

    SECTION("empty directory") {}

    SECTION("directory with layouts that have no image entities") {
        MakeTestLayout(dir.path, "empty", {});
    }

    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromLayoutsDir(dir.path, false, dst));
    CHECK(dst.empty());
}

TEST_CASE("CollectImagesFromLayoutsDir collects images from layouts in directory", "[collect][layouts_dir]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path, "test", { img.path });

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromLayoutsDir(dir.path, false, dst));
    CHECK(dst.size() == 1);
}

TEST_CASE("CollectImagesFromLayoutsDir non-recursive does not enter subdirectories", "[collect][layouts_dir]") {
    TempDir dir;
    std::filesystem::create_directories(dir.path / "sub");
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path / "sub", "test", { img.path });

    std::vector<ImageDetails> dst;
    CHECK_FALSE(CollectImagesFromLayoutsDir(dir.path, false, dst));
}

TEST_CASE("CollectImagesFromLayoutsDir recursive enters subdirectories", "[collect][layouts_dir]") {
    TempDir dir;
    std::filesystem::create_directories(dir.path / "sub");
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path / "sub", "test", { img.path });

    std::vector<ImageDetails> dst;
    REQUIRE(CollectImagesFromLayoutsDir(dir.path, true, dst));
    CHECK(dst.size() == 1);
}
