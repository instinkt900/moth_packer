from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.files import load

class MothPacker(ConanFile):
    name = "moth_packer"

    license = "MIT"
    description = "Standalone texture packer CLI for moth_ui layouts"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps", "MSBuildToolchain", "MSBuildDeps"
    exports_sources = "CMakeLists.txt", "version.txt", "src/*", "external/stb/*"

    def set_version(self):
        self.version = load(self, "version.txt").strip()

    def requirements(self):
        self.requires("moth_ui/1.1.1")
        self.requires("cli11/2.4.2")
        self.requires("spdlog/[~1.14]")

    def build_requirements(self):
        self.tool_requires("cmake/3.27.0")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
