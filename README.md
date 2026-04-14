# Moth Packer

[![Build and Test](https://github.com/instinkt900/moth_packer/actions/workflows/build-test.yml/badge.svg)](https://github.com/instinkt900/moth_packer/actions/workflows/build-test.yml)
[![Release](https://github.com/instinkt900/moth_packer/actions/workflows/upload-release.yml/badge.svg)](https://github.com/instinkt900/moth_packer/actions/workflows/upload-release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A C++17 texture atlas packer for [moth_ui](https://github.com/instinkt900/moth_ui) layouts. Available as both a **C++ library** (for embedding packing into your own tools) and a **command-line tool**. Two operating modes: **pack** images into a bin-packed atlas (standard or flipbook), or **unpack** a sprite sheet back into individual images. Outputs JSON descriptors alongside one or more atlas images (PNG, BMP, TGA, or JPEG).

---

## Table of Contents

- [Features](#features)
  - [AI Disclosure](#ai-disclosure)
- [Usage](#usage)
  - [Modes](#modes)
  - [Input sources](#input-sources)
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

**Two modes** — `pack` bins images into one or more power-of-two atlases (pass `--pack-type flipbook` for a single bin-packed atlas with per-frame rects and a named clip sequence), and `unpack` extracts sprites from a sheet by detecting non-transparent regions.

**Multiple input sources** — point moth_packer at a directory of images, a glob pattern, a plain text file listing image paths, a single moth_ui layout file, or a directory of layout files. Recursive directory traversal is supported for all directory-based sources.

**Automatic atlas sizing** — the packer tests power-of-two atlas dimensions between configurable min and max sizes and picks the most space-efficient fit. When images don't all fit in one atlas, additional atlases are created automatically.

**Single JSON descriptor** — all atlases produced in one run are described in a single `.json` file, making it straightforward to load an entire pack at runtime without enumerating files. Flipbook output (`--pack-type flipbook`) writes a `.flipbook.json` descriptor with per-frame rects, pivots, and named clip sequences.

**Configurable padding** — add a pixel border around each image with four fill modes: solid color (`color`), clamp-to-edge (`extend`), mirrored reflection (`mirror`), or tiling wrap (`wrap`).

**Overwrite protection** — by default the packer refuses to overwrite existing output files and treats all images being skipped (oversized) as an error. Pass `--force` to override both behaviors.

**Dry run** — see what would be packed and what output files would be written without touching the filesystem.

### AI Disclosure

AI agents (primarily Claude) are used as tools in this project for tasks such as refactoring, documentation writing, and test implementation. The architecture, design decisions, and direction of the project are human-driven. This is not a vibe-coded project.

---

## Usage

```
moth_packer <path> [options]
```

In **pack** mode, `<path>` is the base name for the output (no extension). For atlas packing (default) the packer writes `<path>.json` and `<path>_0.<ext>`, `<path>_1.<ext>`, … to the output directory. For flipbook packing (`--pack-type flipbook`) it writes `<path>.flipbook.json` and `<path>.<ext>` (a single atlas image). In **unpack** mode, `<path>` is the input sprite sheet to extract from.

### Modes

| `--mode` | Description |
|---|---|
| `pack` *(default)* | Bin-pack images into one or more power-of-two atlases. Pass `--pack-type flipbook` for a single bin-packed atlas with per-frame rects and a named clip. |
| `unpack` | Extract individual sprites from a sprite sheet by detecting non-transparent regions |

### Input sources

Exactly one input source must be specified for pack mode:

| Flag | Description |
|---|---|
| `-i, --file <path>` | Text file containing one image path per line |
| `-d, --dir <path>` | Directory of images |
| `-g, --glob <pattern>` | Glob pattern (e.g. `assets/**/*.png`) |
| `-l, --layout <path>` | Single moth_ui layout file |
| `-x, --layout-dir <path>` | Directory of moth_ui layout files |

### Options

**General**

| Flag | Default | Description |
|---|---|---|
| `--mode <mode>` | `pack` | Operating mode: `pack` or `unpack` |
| `-o, --out <path>` | `.` | Directory to write output files into |
| `-r, --recursive` | off | Recurse into subdirectories (directory input sources only) |
| `-f, --force` | off | Overwrite existing output files and allow packing to succeed with zero atlases (when all images are oversized) |
| `--dry-run` | off | Report what would be packed without writing any files |
| `--verbose` | off | Enable info-level log messages |
| `--silent` | off | Suppress all output except errors (mutually exclusive with `--verbose`) |
| `-v, --version` | | Print version and exit |

**Output format**

| Flag | Default | Description |
|---|---|---|
| `--format <fmt>` | `png` | Atlas image format: `png`, `bmp`, `tga`, `jpeg` (or `jpg`) |
| `--jpeg-quality <n>` | `90` | JPEG encode quality 1–100 (only used with `--format jpeg`) |
| `--pretty` | off | Pretty-print the JSON descriptor with 4-space indentation |
| `--absolute-paths` | off | Write absolute paths in the JSON descriptor instead of paths relative to the output directory |

**Dimension bounds**

| Flag | Default | Description |
|---|---|---|
| `--min-dim <WxH>` | `256x256` (pack) | pack: minimum atlas dimensions. unpack: minimum sprite size to keep; smaller detected sprites are discarded |
| `--max-dim <WxH>` | `4096x4096` (pack) | pack: maximum atlas dimensions. unpack: maximum sprite size to keep; larger detected sprites are discarded |

**Pack options**

| Flag | Default | Description |
|---|---|---|
| `--pack-type <type>` | `atlas` | Output type: `atlas` (multi-atlas JSON) or `flipbook` (single-atlas `.flipbook.json` with per-frame rects and a clip sequence) |
| `-p, --padding <n>` | `0` | Pixels of padding added around each image on all sides |
| `--padding-type <type>` | `color` | Padding fill mode: `color`, `extend`, `mirror`, or `wrap` |
| `--padding-color <RRGGBBAA>` | `00000000` | Atlas background color as 8 hex digits; fills the entire atlas before compositing |

**Flipbook options** *(only used when `--pack-type flipbook`)*

| Flag | Default | Description |
|---|---|---|
| `--fps <n>` | `12` | Frames per second for the auto-generated clip |
| `--loop <type>` | `loop` | Loop behavior: `loop`, `stop`, or `reset` |
| `--clip-name <name>` | `default` | Name for the auto-generated clip |

**Unpack options**

| Flag | Default | Description |
|---|---|---|
| `--alpha-threshold <n>` | `0` | Pixels with alpha above this value are treated as opaque (0–255). Only used when no background color is active. |
| `--auto-bg` | off | Infer background color by sampling the four corner pixels. Falls back to alpha detection if corners disagree. |
| `--bg-color <RRGGBB>` | — | Explicit background color as a 6-digit hex value (e.g. `FF00FF`). Overrides `--auto-bg` and `--alpha-threshold`. |
| `--color-threshold <n>` | `10` | Per-channel tolerance when comparing pixels against the background color (0–255). |
| `--replace-bg-color <RRGGBBAA>` | — | Replace detected background pixels in each extracted sprite with this color. Use `00000000` for full transparency. Requires a background detection mode. |

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
moth_packer sprites -i sprites.txt --max-dim 2048x2048 -o build/atlases
```

Pack images matching a glob pattern:
```bash
moth_packer sprites -g 'assets/**/*.png' -o build/atlases
```

Pack with 2-pixel extend padding and pretty-printed JSON:
```bash
moth_packer ui -d assets/images -p 2 --padding-type extend --pretty -o build/atlases
```

Build a flipbook animation sheet from a directory of frames:
```bash
moth_packer explosion --pack-type flipbook -d assets/explosion --fps 24 -o build/atlases
```

Extract sprites from a sprite sheet (transparent background):
```bash
moth_packer sheet.png --mode unpack -o sprites/
```

Extract sprites ignoring near-transparent edge pixels (e.g. from JPEG compression):
```bash
moth_packer sheet.png --mode unpack --alpha-threshold 10 -o sprites/
```

Extract sprites from a sheet with a uniform solid background (auto-detect color from corners):
```bash
moth_packer sheet.png --mode unpack --auto-bg -o sprites/
```

Extract sprites from a sheet with a known magenta background:
```bash
moth_packer sheet.png --mode unpack --bg-color FF00FF -o sprites/
```

Extract sprites, discarding stray-pixel noise smaller than 8×8:
```bash
moth_packer sheet.png --mode unpack --min-dim 8x8 -o sprites/
```

Extract sprites from a sheet with a magenta background, replacing that background with transparency:
```bash
moth_packer sheet.png --mode unpack --bg-color FF00FF --replace-bg-color 00000000 -o sprites/
```

Preview what would be packed without writing anything:
```bash
moth_packer ui -d assets/images --dry-run --verbose
```

---

## Output format

### Pack

Each pack run produces one `<name>.json` descriptor and one or more `<name>_N.<ext>` atlas images. The descriptor lists every atlas and the images packed into it, with their source path and pixel rect:

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

Paths are relative to the output directory by default. Pass `--absolute-paths` to write absolute paths instead.

### Flipbook

A flipbook run produces `<name>.<ext>` (the atlas image) and `<name>.flipbook.json`. The descriptor lists each packed frame with its pixel rect and pivot, and one or more named clip sequences:

```json
{
  "image": "explosion.png",
  "frames": [
    { "x": 0,   "y": 0, "w": 64, "h": 64, "pivot_x": 0, "pivot_y": 0 },
    { "x": 64,  "y": 0, "w": 64, "h": 64, "pivot_x": 0, "pivot_y": 0 },
    { "x": 128, "y": 0, "w": 64, "h": 64, "pivot_x": 0, "pivot_y": 0 }
  ],
  "clips": [
    {
      "name": "default",
      "loop": "loop",
      "frames": [
        { "frame": 0, "duration_ms": 42 },
        { "frame": 1, "duration_ms": 41 },
        { "frame": 2, "duration_ms": 42 }
      ]
    }
  ]
}
```

`loop` is one of `loop`, `stop`, or `reset`. Frame durations sum to `round(frameCount * 1000 / fps)` ms.

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

Pass `-o moth_packer/*:build_cli=True` to build the `moth_packer` executable alongside the library.

**Linux:**
```bash
conan install . -pr .conan/profile -o moth_packer/*:build_cli=True -s build_type=Release --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
```

**Windows:**
```bash
conan install . -pr .conan/profile -o moth_packer/*:build_cli=True -s build_type=Release --build=missing
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
