# Changelog

All notable changes to this project will be documented in this file.
Entries are generated automatically from git history using [git-cliff](https://github.com/orhun/git-cliff).

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


