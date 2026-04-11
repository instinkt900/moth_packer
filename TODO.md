# TODO

### Named Clip Metadata Input

**Effort:** Medium

Support an input metadata file that maps clip names to ordered lists of image paths, allowing
multiple named clips to be packed into a single atlas without relying on filename conventions.
Currently the flipbook path auto-generates a single clip from alphabetically sorted inputs;
a metadata file would let callers define any number of clips with explicit frame ordering and
per-clip loop/fps settings. Closely related to the in-memory API item below.

---

### In-Memory Pack API

**Effort:** Medium

The current API is entirely file-in / file-out: `ImageDetails` holds only a filesystem path,
`Pack()` loads images from disk, and output is always written as image + JSON files. This makes
it awkward to drive packing from a host application (such as the sprite editor) where images
are already resident in memory.

Three concrete flows are needed:

1. **In-memory image input** — extend `ImageDetails` (or add a parallel struct) to carry raw
   pixel data (`width × height × channels` bytes) and a logical name, so callers can pass
   already-loaded images without touching the filesystem. Per-image associative metadata should
   also be expressible here: clip membership, frame order within that clip, and an optional pivot
   hint — enough for `Pack()` to auto-generate correct frame and clip descriptor entries without
   a separate metadata file.

2. **In-memory output** — add a result struct returned from `Pack()` (instead of, or alongside,
   writing files) that carries the packed atlas as a raw pixel buffer and the descriptor as a
   typed C++ structure rather than a JSON string on disk. This lets the caller display the atlas
   immediately and inspect or further edit the descriptor without round-tripping through the
   filesystem.

3. **Repack flow** — given an existing in-memory atlas and its flipbook descriptor, extract each
   frame's pixel data using the rects in the descriptor (an internal unpack step), run the
   extracted frames back through the packing algorithm, and return a new atlas buffer and a new
   descriptor with all frame rects updated to their new positions. This is needed when a user
   modifies an existing sprite sheet in the editor — adding, removing, or reordering frames —
   and wants a clean re-optimised atlas without manually separating frames first. There is
   currently no logical path for this: `Unpack()` writes to disk and uses heuristic detection
   rather than descriptor rects, so there is no way to round-trip an atlas through the packer
   in memory.

   A related variant is packing multiple separate in-memory images alongside an existing
   descriptor into a single new atlas. The named-image-input flow above handles the image side,
   but the caller also needs to be able to supply a pre-existing descriptor (or a partial one)
   so that clip structure, pivot points, and frame metadata accumulated in the editor are
   preserved across the repack rather than regenerated from scratch.

All additions should keep the existing file-based path working unchanged.

---

### Descriptor-Driven Unpack

**Effort:** Medium

Extend unpack mode to accept a pack descriptor JSON (`--metadata <path>`) and use the rect data
to extract each named image precisely, making it the exact inverse of pack mode. Currently unpack
detects sprites heuristically from pixel data; descriptor-driven unpack would be lossless and
name-preserving.
