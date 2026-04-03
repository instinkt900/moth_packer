# moth_packer TODO

- Consider supporting an input metadata file that maps clip names to ordered lists of image paths, allowing multiple named clips to be packed into a single atlas without relying on filename conventions.
- `--max-images <N>` — cap the number of images per atlas
- Expand unpack mode to accept a pack descriptor JSON (e.g. `--metadata <path>`) and use the rect data to extract each named image precisely, making it the exact inverse of pack mode. Currently unpack detects sprites heuristically from pixel data; descriptor-driven unpack would be lossless and name-preserving.
