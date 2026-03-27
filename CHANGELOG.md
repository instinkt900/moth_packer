# Changelog

All notable changes to this project will be documented in this file.
Entries are generated automatically from git history using [git-cliff](https://github.com/orhun/git-cliff).

## [0.3.0] - 2026-03-27
### Features
- Implement padding fill in CommitPack (color, extend, mirror, wrap)
- Add glob input source (-g/--glob) to CLI and packer
- Replace hand-rolled glob with p-ranav/glob header library
- Add --pretty flag to pretty print output JSON
- Add --absolute-paths flag to write absolute paths in output JSON
- Treat zero packed images as an error unless --force is given
- Add deprecated positional Pack() overload for backwards compatibility
- Add --format and --jpeg-quality options for atlas output format

### Bug Fixes
- Add glob include path to moth_packer library target
- Validate PackOptions::filename is non-empty in Pack()
- Correct expand_tilde to use HOME/USERPROFILE instead of USER/USERNAME
- Guard rlistdir against symlink cycles using a visited canonical path set
- Treat stbi_load failure in CommitPack as a fatal error
- Validate PackOptions dimensions and padding in Pack()
- Widen area calculations to int64_t to prevent overflow
- Correct Mirror padding to use 2*size period for arbitrary depth
- Use glob::rglob in CollectImagesFromGlob to enable ** expansion
- Propagate JSON descriptor write failures in Pack()
- Remove extra braces in Pack() calls in test_packer.cpp
- Check ifstream is_open in CollectImagesFromFile
- Export external/glob sources in conanfile.py
- Warn on unsupported image formats in glob and dir collection modes
- Normalise file extensions to lowercase before format check
- Validate jpegQuality in Pack() and add default case to FormatExtension

### Refactoring
- Replace Pack argument list with PackOptions struct
- Replace [[deprecated]] attribute with doc comment on Pack() stub
- Pre-fill atlas with paddingColor as background colour

### Documentation
- Add public API docstrings, fix stale CommitPack comment, bool const& param
- Update README for glob, padding, --pretty, and --absolute-paths
- Correct Pack() @return to reflect partial-success semantics
- Fix escaped slash in CollectImagesFromGlob example pattern
- Update README to reflect configurable atlas output format

### Testing
- Update Pack calls to use PackOptions struct

### Miscellaneous
- Add MIT license for vendored p-ranav/glob
- Update TODO and tweaking clang-format config

### Changes
- Update src/packer.cpp
- Bump version from 0.2.1 to 0.3.0

## [0.2.1] - 2026-03-25
### Bug Fixes
- Force-refresh tags on fetch to avoid stale refs on retry
- Use ../.conan/profile in tests subdirectory

### Miscellaneous
- Adjusting moth_ui version requirements

## [0.2.0] - 2026-03-22
### Bug Fixes
- Fetch tags after creation so git-cliff --current finds the tag

### Changes
- Clarify build instructions in README.md

## [0.1.1] - 2026-03-22
### Features
- Add test project skeleton
- Add Pack tests
- Add JSON path tests and silence spdlog during tests
- Add Collect tests and fix duplicate detection bug
- Add additional Pack and Collect tests
- Add build_cli conan option, default false

### Bug Fixes
- Use PID in TempDir names to prevent parallel test collisions
- Check stbi_write_png result and pass explicit Debug config to cmake/ctest
- Mark TempDir destructor noexcept
- Validate positive dimensions in MakeTestImage
- Restore cli11 conan requirement for local CLI builds
- Complete install rules for library export and CLI executable
- Add using moth_packer::Pack to test file for unqualified calls
- Explicitly declare moth_packer as STATIC library

### Refactoring
- Split moth_packer into library and CLI, add moth_packer namespace
- Gate CLI build and CLI11 dep behind MOTH_PACKER_BUILD_CLI option

### Documentation
- Add full ecosystem table to Related Projects
- Update canyon link to moth_graphics in Related Projects

### Testing
- Fixed small issue by using non-throwing version of remove_all

### Miscellaneous
- Bump version to 0.2.0

### Changes
- Bump version from 0.1.0 to 0.1.1

## [0.1.0] - 2026-03-21
### Features
- Initial moth_packer CLI tool
- Add output name argument and optional output directory flag
- Expand input sources and refactor collection API
- Add --force flag and overwrite protection to Pack
- Add version to help text and -v/--version flag
- Add --verbose, --silent and --dry-run flags

### Bug Fixes
- Guard FindOptimalDimensions against empty candidate list and zero-ratio results

### Refactoring
- Consolidate pack output to single JSON descriptor referencing multiple atlas PNGs

### Documentation
- Add README, LICENSE and TODO
- Add CI badges to README

### Miscellaneous
- Removing unused conan profiles and updating docs

### Changes
- Fix README title casing and formatting


