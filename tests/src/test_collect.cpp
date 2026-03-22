#include <catch2/catch_all.hpp>

#include "moth_packer/packer.h"
#include "test_helpers.h"

#include <filesystem>
#include <fstream>

// ---------------------------------------------------------------------------
// moth_packer::CollectImagesFromFile
// ---------------------------------------------------------------------------

TEST_CASE("moth_packer::CollectImagesFromFile returns false for non-existent file", "[collect][file]") {
    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromFile("/nonexistent/list.txt", dst));
}

TEST_CASE("moth_packer::CollectImagesFromFile returns false when no valid images in file", "[collect][file]") {
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

    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromFile(listPath, dst));
    CHECK(dst.empty());
}

TEST_CASE("moth_packer::CollectImagesFromFile collects valid images", "[collect][file]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);

    auto const listPath = dir.path / "list.txt";
    std::ofstream(listPath) << img.path.string() << "\n";

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromFile(listPath, dst));
    REQUIRE(dst.size() == 1);
    CHECK(dst[0].path == img.path);
    CHECK(dst[0].dimensions.x == 16);
    CHECK(dst[0].dimensions.y == 16);
}

TEST_CASE("moth_packer::CollectImagesFromFile skips duplicate entries", "[collect][file]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);

    auto const listPath = dir.path / "list.txt";
    {
        std::ofstream f(listPath);
        f << img.path.string() << "\n";
        f << img.path.string() << "\n"; // duplicate
    }

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromFile(listPath, dst));
    CHECK(dst.size() == 1);
}

// ---------------------------------------------------------------------------
// moth_packer::CollectImagesFromDir
// ---------------------------------------------------------------------------

TEST_CASE("moth_packer::CollectImagesFromDir returns false for non-existent directory", "[collect][dir]") {
    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromDir("/nonexistent/dir", false, dst));
}

TEST_CASE("moth_packer::CollectImagesFromDir returns false when directory has no supported images", "[collect][dir]") {
    TempDir dir;

    SECTION("empty directory") {}

    SECTION("directory with only unsupported files") {
        std::ofstream(dir.path / "doc.txt").close();
        std::ofstream(dir.path / "image.psd").close();
    }

    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromDir(dir.path, false, dst));
    CHECK(dst.empty());
}

TEST_CASE("moth_packer::CollectImagesFromDir collects images from flat directory", "[collect][dir]") {
    TempDir dir;
    MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestImage(dir.path, "b.png", 32, 32);

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromDir(dir.path, false, dst));
    CHECK(dst.size() == 2);
}

TEST_CASE("moth_packer::CollectImagesFromDir non-recursive does not enter subdirectories", "[collect][dir]") {
    TempDir dir;
    MakeTestImage(dir.path, "a.png", 16, 16);
    std::filesystem::create_directories(dir.path / "sub");
    MakeTestImage(dir.path / "sub", "b.png", 16, 16);

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromDir(dir.path, false, dst));
    CHECK(dst.size() == 1);
}

TEST_CASE("moth_packer::CollectImagesFromDir recursive enters subdirectories", "[collect][dir]") {
    TempDir dir;
    MakeTestImage(dir.path, "a.png", 16, 16);
    std::filesystem::create_directories(dir.path / "sub");
    MakeTestImage(dir.path / "sub", "b.png", 16, 16);

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromDir(dir.path, true, dst));
    CHECK(dst.size() == 2);
}

TEST_CASE("moth_packer::CollectImagesFromDir skips images already in destination list", "[collect][dir]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);

    std::vector<moth_packer::ImageDetails> dst = { img }; // pre-populate with the same image
    CHECK_FALSE(moth_packer::CollectImagesFromDir(dir.path, false, dst)); // returns false — nothing new found
    CHECK(dst.size() == 1);
}

// ---------------------------------------------------------------------------
// moth_packer::CollectImagesFromLayout
// ---------------------------------------------------------------------------

TEST_CASE("moth_packer::CollectImagesFromLayout returns false for non-existent layout", "[collect][layout]") {
    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromLayout("/nonexistent/layout.moth_layout", dst));
}

TEST_CASE("moth_packer::CollectImagesFromLayout returns false for layout with no image entities", "[collect][layout]") {
    TempDir dir;
    auto const layoutPath = MakeTestLayout(dir.path, "empty", {});

    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromLayout(layoutPath, dst));
    CHECK(dst.empty());
}

TEST_CASE("moth_packer::CollectImagesFromLayout collects images referenced by layout", "[collect][layout]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    auto const layoutPath = MakeTestLayout(dir.path, "test", { img.path });

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromLayout(layoutPath, dst));
    REQUIRE(dst.size() == 1);
    CHECK(dst[0].path == img.path);
}

TEST_CASE("moth_packer::CollectImagesFromLayout returns false for corrupt layout file", "[collect][layout]") {
    TempDir dir;
    auto const layoutPath = dir.path / ("corrupt" + moth_ui::Layout::FullExtension);
    std::ofstream(layoutPath) << "this is not valid json {{{";

    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromLayout(layoutPath, dst));
    CHECK(dst.empty());
}

TEST_CASE("moth_packer::CollectImagesFromLayout returns false when referenced image is missing on disk", "[collect][layout]") {
    TempDir dir;
    auto const missingImagePath = dir.path / "missing.png"; // never created
    auto const layoutPath = MakeTestLayout(dir.path, "test", { missingImagePath });

    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromLayout(layoutPath, dst));
    CHECK(dst.empty());
}

TEST_CASE("moth_packer::CollectImagesFromLayout collects multiple distinct images", "[collect][layout]") {
    TempDir dir;
    auto const imgA = MakeTestImage(dir.path, "a.png", 16, 16);
    auto const imgB = MakeTestImage(dir.path, "b.png", 32, 32);
    auto const layoutPath = MakeTestLayout(dir.path, "test", { imgA.path, imgB.path });

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromLayout(layoutPath, dst));
    CHECK(dst.size() == 2);
}

TEST_CASE("moth_packer::CollectImagesFromLayout skips duplicate images across entities", "[collect][layout]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    // same image referenced twice in the layout
    auto const layoutPath = MakeTestLayout(dir.path, "test", { img.path, img.path });

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromLayout(layoutPath, dst));
    CHECK(dst.size() == 1);
}

// ---------------------------------------------------------------------------
// moth_packer::CollectImagesFromLayoutsDir
// ---------------------------------------------------------------------------

TEST_CASE("moth_packer::CollectImagesFromLayoutsDir returns false for non-existent directory", "[collect][layouts_dir]") {
    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromLayoutsDir("/nonexistent/dir", false, dst));
}

TEST_CASE("moth_packer::CollectImagesFromLayoutsDir returns false when no images found", "[collect][layouts_dir]") {
    TempDir dir;

    SECTION("empty directory") {}

    SECTION("directory with layouts that have no image entities") {
        MakeTestLayout(dir.path, "empty", {});
    }

    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromLayoutsDir(dir.path, false, dst));
    CHECK(dst.empty());
}

TEST_CASE("moth_packer::CollectImagesFromLayoutsDir collects images from layouts in directory", "[collect][layouts_dir]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path, "test", { img.path });

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromLayoutsDir(dir.path, false, dst));
    CHECK(dst.size() == 1);
}

TEST_CASE("moth_packer::CollectImagesFromLayoutsDir non-recursive does not enter subdirectories", "[collect][layouts_dir]") {
    TempDir dir;
    std::filesystem::create_directories(dir.path / "sub");
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path / "sub", "test", { img.path });

    std::vector<moth_packer::ImageDetails> dst;
    CHECK_FALSE(moth_packer::CollectImagesFromLayoutsDir(dir.path, false, dst));
}

TEST_CASE("moth_packer::CollectImagesFromLayoutsDir deduplicates images shared across layouts", "[collect][layouts_dir]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path, "layout1", { img.path });
    MakeTestLayout(dir.path, "layout2", { img.path });

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromLayoutsDir(dir.path, false, dst));
    CHECK(dst.size() == 1);
}

TEST_CASE("moth_packer::CollectImagesFromLayoutsDir skips corrupt layouts and collects from valid ones", "[collect][layouts_dir]") {
    TempDir dir;
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path, "valid", { img.path });
    std::ofstream(dir.path / ("corrupt" + moth_ui::Layout::FullExtension)) << "not valid json {{{";

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromLayoutsDir(dir.path, false, dst));
    CHECK(dst.size() == 1);
}

TEST_CASE("moth_packer::CollectImagesFromLayoutsDir recursive enters subdirectories", "[collect][layouts_dir]") {
    TempDir dir;
    std::filesystem::create_directories(dir.path / "sub");
    auto const img = MakeTestImage(dir.path, "a.png", 16, 16);
    MakeTestLayout(dir.path / "sub", "test", { img.path });

    std::vector<moth_packer::ImageDetails> dst;
    REQUIRE(moth_packer::CollectImagesFromLayoutsDir(dir.path, true, dst));
    CHECK(dst.size() == 1);
}
