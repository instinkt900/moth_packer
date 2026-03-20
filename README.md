# Moth Packer

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A standalone command-line tool for packing images into texture atlases. Supports plain image directories, file lists, and [moth_ui](https://github.com/instinkt900/moth_ui) layout files as input sources. Outputs a single JSON descriptor alongside one or more atlas PNGs.

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
  - [Linux](#linux)
  - [Windows](#windows)
- [Related Projects](#related-projects)
- [License](#license)

---

## Features

**Multiple input modes** — point moth_packer at a directory of images, a plain text file listing image paths, a single moth_ui layout file, or a directory of layout files. Recursive directory traversal is supported for all directory-based modes.

**Automatic atlas sizing** — the packer tests power-of-two atlas dimensions between configurable min and max sizes and picks the most space-efficient fit. When images don't all fit in one atlas, additional atlases are created automatically.

**Single JSON descriptor** — all atlases produced in one run are described in a single `.json` file, making it straightforward to load an entire pack at runtime without enumerating files.

**Overwrite protection** — by default the packer refuses to overwrite existing output files. Pass `--force` to override.

**Dry run** — see what would be packed and what output files would be written without touching the filesystem.

### AI Disclosure

AI agents (primarily Claude) are used as tools in this project for tasks such as refactoring, documentation writing, and test implementation. The architecture, design decisions, and direction of the project are human-driven. This is not a vibe-coded project.

---

## Usage

```
moth_packer <output> [options]
```

`<output>` is the base name for the pack (no extension). The packer writes `<output>.json` and `<output>_0.png`, `<output>_1.png`, … to the output directory.

### Input modes

Exactly one input mode must be specified:

| Flag | Description |
|---|---|
| `-i, --file <path>` | Text file containing one image path per line |
| `-d, --dir <path>` | Directory of images |
| `-l, --layout <path>` | Single moth_ui layout file |
| `-x, --layout-dir <path>` | Directory of moth_ui layout files |

### Options

| Flag | Default | Description |
|---|---|---|
| `-o, --out <path>` | `.` | Directory to write output files into |
| `-r, --recursive` | off | Recurse into subdirectories (directory input modes only) |
| `-f, --force` | off | Overwrite existing output files |
| `-n, --min <w,h>` | `256,256` | Minimum atlas dimensions |
| `-m, --max <w,h>` | `4096,4096` | Maximum atlas dimensions |
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

Preview what would be packed without writing anything:
```bash
moth_packer ui -d assets/images --dry-run --verbose
```

---

## Output format

Each run produces one `<name>.json` descriptor and one or more `<name>_N.png` atlas images. The descriptor lists every atlas and the images packed into it, with their source path and pixel rect within the atlas:

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

All paths in the descriptor are relative to the output directory.

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

Conan profiles for both platforms are provided in `conan/profiles/`.

### Linux

```bash
conan install . --profile conan/profiles/linux_profile --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
```

### Windows

```bash
conan install . --profile conan/profiles/windows_profile --build=missing -s build_type=Release
cmake --preset conan-default
cmake --build --preset conan-release
```

---

## Related Projects

| Project | Description |
|---|---|
| [moth_ui](https://github.com/instinkt900/moth_ui) | Core UI library — node graph, keyframe animation, and event system |
| [moth_editor](https://github.com/instinkt900/moth_editor) | Visual layout and animation editor with built-in texture packing |

---

## License

MIT
