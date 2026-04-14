# TODO

## Named Clip Metadata Input

**Effort:** Medium

Support an input metadata file that maps clip names to ordered lists of image paths, allowing
multiple named clips to be packed into a single atlas without relying on filename conventions.
Currently the flipbook path auto-generates a single clip from alphabetically sorted inputs;
a metadata file would let callers define any number of clips with explicit frame ordering and
per-clip loop/fps settings. Closely related to the in-memory API item below.

---

## Descriptor-Driven Unpack

**Effort:** Medium

Extend unpack to accept a pack descriptor (JSON file or in-memory `PackResult`) and use the rect
data to extract each named image precisely, making it the exact inverse of pack mode. Currently
unpack detects sprites heuristically from pixel data; descriptor-driven unpack would be lossless
and name-preserving. Also required as a prerequisite for the Repack Flow below.

---

## Repack Flow

**Effort:** Medium  
**Depends on:** Descriptor-Driven Unpack, In-Memory Pack API (complete)

Given an existing atlas pixel buffer and its descriptor (`PackResult` or JSON), slice each frame
back out using the known rects (descriptor-driven unpack step), then feed the extracted
`ImageInput` frames back through `PackToMemory` to produce a new optimised atlas and updated
descriptor. This is needed when a user modifies a sprite sheet in the editor — adding, removing,
or reordering frames — and wants a clean re-packed atlas without manually separating frames first.
