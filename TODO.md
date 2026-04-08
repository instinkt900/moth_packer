# TODO

### Sprite Animation Redesign

**Effort:** Large

The current `flipbook` mode has two significant limitations and should be folded into `pack` mode,
leaving only `pack` and `unpack` as top-level modes.

Problems:
1. **Fixed frame size** — all frames share a single cell dimension. Real animations often contain
   frames of different sizes (e.g. a crouch frame is shorter than a standing frame).
2. **Range-based clips** — clips defined as `{ start, end }` ranges prevent repeating frames,
   frame sharing between clips, and non-linear sequences. Clips should instead be explicit ordered
   lists of frame indices, e.g. `"frames": [0, 1, 2, 1, 0]`.

Proposed model:
- Frames are bin-packed individually (like the existing `pack` mode) with their own rects in the
  atlas — no uniform grid.
- The descriptor contains a named frames array (each entry with a rect and a pivot point) and a
  clips array (each clip references frames by index with fps and loop behavior).
- Animation metadata is driven by an input metadata file mapping clip names to ordered lists of
  image paths.

Open question — per-frame pivot points: when frame sizes differ, the runtime needs a shared
reference point to align frames correctly. Options:
- **Named anchor presets** (`bottom-center`, `center`, etc.) as a convenient shorthand.
- **Per-frame explicit pivot coordinates** for full control (e.g. a lunge where the foot shifts
  horizontally).
- Possibly both: a default anchor preset overridable per frame.

---

### Named Clip Metadata Input

**Effort:** Medium

Support an input metadata file that maps clip names to ordered lists of image paths, allowing
multiple named clips to be packed into a single atlas without relying on filename conventions.
Closely related to and likely part of the sprite animation redesign above.

---

### Descriptor-Driven Unpack

**Effort:** Medium

Extend unpack mode to accept a pack descriptor JSON (`--metadata <path>`) and use the rect data
to extract each named image precisely, making it the exact inverse of pack mode. Currently unpack
detects sprites heuristically from pixel data; descriptor-driven unpack would be lossless and
name-preserving.
