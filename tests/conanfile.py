from conan import ConanFile
from conan.tools.cmake import cmake_layout


class MothPackerTests(ConanFile):
    name = "moth_packer_tests"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("catch2/3.13.0")
        # moth_packer dependencies (packer.cpp built from source via direct include)
        self.requires("moth_ui/1.1.1")
        self.requires("range-v3/[~0.12]")
        self.requires("nlohmann_json/[~3.11]")
        self.requires("fmt/[~10.2]")
        self.requires("spdlog/[~1.14]")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.27.0]")

    def layout(self):
        cmake_layout(self)
