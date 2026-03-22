from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeToolchain
from conan.tools.files import load

class MothPacker(ConanFile):
    name = "moth_packer"

    license = "MIT"
    description = "Texture packer library and CLI for moth_ui layouts"

    settings = "os", "compiler", "build_type", "arch"
    package_type = "static-library"
    options = {"build_cli": [True, False]}
    default_options = {"build_cli": False}
    generators = "CMakeDeps", "MSBuildToolchain", "MSBuildDeps"
    exports_sources = "CMakeLists.txt", "version.txt", "include/*", "src/*", "external/stb/*"

    def set_version(self):
        self.version = load(self, "version.txt").strip()

    def requirements(self):
        self.requires("moth_ui/1.1.1", transitive_headers=True)
        self.requires("spdlog/[~1.14]")
        if self.options.build_cli:
            self.requires("cli11/2.4.2", visible=False)

    def build_requirements(self):
        self.tool_requires("cmake/3.27.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["MOTH_PACKER_BUILD_CLI"] = bool(self.options.build_cli)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["moth_packer"]
        self.cpp_info.includedirs = ["include"]
