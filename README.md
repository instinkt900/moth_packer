# Moth Packer

[![Build and Test](https://github.com/instinkt900/moth_packer/actions/workflows/build-test.yml/badge.svg)](https://github.com/instinkt900/moth_packer/actions/workflows/build-test.yml)
[![Release](https://github.com/instinkt900/moth_packer/actions/workflows/upload-release.yml/badge.svg)](https://github.com/instinkt900/moth_packer/actions/workflows/upload-release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A C++17 texture atlas packer for [moth_ui](https://github.com/instinkt900/moth_ui) layouts. Available as both a **C++ library** (for embedding packing into your own tools) and a **command-line tool**. Supports plain image directories, file lists, and moth_ui layout files as input sources. Outputs a single JSON descriptor alongside one or more atlas images (PNG, BMP, TGA, or JPEG).

---

## Table of Contents

- [Features](#features)
  - [AI Disclosure](#ai-disclosure)
- [Usage](#usage)
  - [Input modes](#input-modes)
  - [Options](#options)
  - [Examples](#examples)
- [Output format](#output-format)
- [Building](#building)
  - [Prerequisites](#prerequisites)
  - [Library mode](#library-mode)
  - [CLI mode](#cli-mode)
- [Related Projects](#related-projects)
- [License](#license)

---

## Features

**Multiple input modes** — point moth_packer at a directory of images, a glob pattern, a plain text file listing image paths, a single moth_ui layout file, or a directory of layout files. Recursive directory traversal is supported for all directory-based modes.

**Automatic atlas sizing** — the packer tests power-of-two atlas dimensions between configurable min and max sizes and picks the most space-efficient fit. When images don't all fit in one atlas, additional atlases are created automatically.

**Single JSON descriptor** — all atlases produced in one run are described in a single `.json` file, making it straightforward to load an entire pack at runtime without enumerating files.

**Configurable padding** — add a pixel border around each image with four fill modes: solid colour (`color`), clamp-to-edge (`extend`), mirrored reflection (`mirror`), or tiling wrap (`wrap`).

**Overwrite protection** — by default the packer refuses to overwrite existing output files and treats all images being skipped (oversized) as an error. Pass `--force` to override both behaviours.

**Dry run** — see what would be packed and what output files would be written without touching the filesystem.

### AI Disclosure

AI agents (primarily Claude) are used as tools in this project for tasks such as refactoring, documentation writing, and test implementation. The architecture, design decisions, and direction of the project are human-driven. This is not a vibe-coded project.

---

## Usage

```
moth_packer <output> [options]
```

`<output>` is the base name for the pack (no extension). The packer writes `<output>.json` and `<output>_0.<ext>`, `<output>_1.<ext>`, … to the output directory, where `<ext>` is determined by `--format` (default `png`).

### Input modes

Exactly one input mode must be specified:

| Flag | Description |
|---|---|
| `-i, --file <path>` | Text file containing one image path per line |
| `-d, --dir <path>` | Directory of images |
| `-g, --glob <pattern>` | Glob pattern (e.g. `assets/**/*.png`) |
| `-l, --layout <path>` | Single moth_ui layout file |
| `-x, --layout-dir <path>` | Directory of moth_ui layout files |

### Options

| Flag | Default | Description |
|---|---|---|
| `-o, --out <path>` | `.` | Directory to write output files into |
| `-r, --recursive` | off | Recurse into subdirectories (directory input modes only) |
| `-f, --force` | off | Overwrite existing output files and allow packing to succeed with zero atlases (when all images are oversized) |
| `-n, --min <w,h>` | `256,256` | Minimum atlas dimensions |
| `-m, --max <w,h>` | `4096,4096` | Maximum atlas dimensions |
| `-p, --padding <n>` | `0` | Pixels of padding added around each image on all sides |
| `-t, --padding-type <type>` | `color` | Padding fill mode: `color`, `extend`, `mirror`, or `wrap` |
| `-c, --padding-color <RRGGBBAA>` | `00000000` | Atlas background colour as 8 hex digits; fills the entire atlas before compositing, controlling padding regions and unpacked areas |
| `--format <fmt>` | `png` | Atlas image format: `png`, `bmp`, `tga`, `jpeg` (or `jpg`) |
| `--jpeg-quality <n>` | `90` | JPEG encode quality 1–100 (only used with `--format jpeg`) |
| `--pretty` | off | Pretty-print the JSON descriptor with 4-space indentation |
| `--absolute-paths` | off | Write absolute paths in the JSON descriptor instead of paths relative to the output directory |
| `--dry-run` | off | Report what would be packed without writing any files |
| `--verbose` | off | Print a line for every image packed |
| `--silent` | off | Suppress all output except errors |
| `-v, --version` | | Print version and exit |

### Examples

Pack all PNGs in a directory into a single atlas named `ui`:
```bash
moth_packer ui -d assets/images -o build/atlases
```

Pack only the images referenced by a moth_ui layout:
```bash
moth_packer hud -l layouts/hud.moth_layout -o build/atlases
```

Pack images from a text file list, allowing up to 2048×2048 atlases:
```bash
moth_packer sprites -i sprites.txt -m 2048,2048 -o build/atlases
```

Pack images matching a glob pattern:
```bash
moth_packer sprites -g 'assets/**/*.png' -o build/atlases
```

Pack with 2-pixel extend padding and pretty-printed JSON:
```bash
moth_packer ui -d assets/images -p 2 -t extend --pretty -o build/atlases
```

Preview what would be packed without writing anything:
```bash
moth_packer ui -d assets/images --dry-run --verbose
```

---

## Output format

Each run produces one `<name>.json` descriptor and one or more `<name>_N.<ext>` atlas images (extension matches `--format`, default `png`). The descriptor lists every atlas and the images packed into it, with their source path and pixel rect within the atlas:

```json
{
  "atlases": [
    {
      "atlas": "ui_0.png",
      "images": [
        {
          "path": "assets/images/button.png",
          "rect": { "x": 0, "y": 0, "w": 64, "h": 32 }
        },
        {
          "path": "assets/images/icon.png",
          "rect": { "x": 64, "y": 0, "w": 32, "h": 32 }
        }
      ]
    }
  ]
}
```

Paths in the descriptor are relative to the output directory by default. Pass `--absolute-paths` to write absolute paths instead.

---

## Building

### Prerequisites

Set up a Python virtual environment and install Conan:

```bash
# Linux / macOS
python3 -m venv .venv
source .venv/bin/activate
pip install conan

# Windows (PowerShell)
python3 -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install conan
```

**C++17 is required.** A `.conan/profile` is provided that sets `compiler.cppstd=17`. This profile is used in CI and can be used directly or as a reference when building locally.

### Library mode

The default build produces the static library only. This is the mode used when moth_packer is consumed as a Conan package by another project.

**Linux:**
```bash
conan install . -pr .conan/profile -s build_type=Release --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
```

**Windows:**
```bash
conan install . -pr .conan/profile -s build_type=Release --build=missing
cmake --preset conan-default
cmake --build --preset conan-release
```

### CLI mode

Pass `-o moth_packer:build_cli=True` to build the `moth_packer` executable alongside the library.

**Linux:**
```bash
conan install . -pr .conan/profile -o moth_packer:build_cli=True -s build_type=Release --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
```

**Windows:**
```bash
conan install . -pr .conan/profile -o moth_packer:build_cli=True -s build_type=Release --build=missing
cmake --preset conan-default
cmake --build --preset conan-release
```

---

## Related Projects

| Project | Description |
|---|---|
| [moth_ui](https://github.com/instinkt900/moth_ui) | Core UI library — node graph, keyframe animation, and event system |
| [moth_graphics](https://github.com/instinkt900/moth_graphics) | Graphics and application framework built on moth_ui — SDL2 and Vulkan backends, window management, and a layer stack |
| [moth_editor](https://github.com/instinkt900/moth_editor) | Visual layout and animation editor — Flash-like authoring tool for creating moth_ui layout files |
| moth_packer | *(this project)* Command-line texture atlas packer for images and moth_ui layouts |

---

## License

MIT
