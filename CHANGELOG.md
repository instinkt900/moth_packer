# Changelog

All notable changes to this project will be documented in this file.
Entries are generated automatically from git history using [git-cliff](https://github.com/orhun/git-cliff).

## [0.1.1] - 2026-03-22
### Bug Fixes
- Use PID in TempDir names to prevent parallel test collisions
- Check stbi_write_png result and pass explicit Debug config to cmake/ctest
- Mark TempDir destructor noexcept
- Validate positive dimensions in MakeTestImage

### Changes
- Use repository variable for ARTIFACTORY_URL
- Bump version from 0.1.0 to 0.1.1

### Documentation
- Add full ecosystem table to Related Projects
- Update canyon link to moth_graphics in Related Projects

### Features
- Add test project skeleton
- Add Pack tests
- Add JSON path tests and silence spdlog during tests
- Add Collect tests and fix duplicate detection bug
- Add additional Pack and Collect tests

### Testing
- Fixed small issue by using non-throwing version of remove_all

## [0.1.0] - 2026-03-21
### Bug Fixes
- Guard FindOptimalDimensions against empty candidate list and zero-ratio results

### Changes
- Fix README title casing and formatting
- Add build, upload, and release workflows

### Documentation
- Add README, LICENSE and TODO
- Add CI badges to README

### Features
- Initial moth_packer CLI tool
- Add output name argument and optional output directory flag
- Expand input sources and refactor collection API
- Add --force flag and overwrite protection to Pack
- Add version to help text and -v/--version flag
- Add --verbose, --silent and --dry-run flags

### Miscellaneous
- Removing unused conan profiles and updating docs

### Refactoring
- Consolidate pack output to single JSON descriptor referencing multiple atlas PNGs


