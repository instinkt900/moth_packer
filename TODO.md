# moth_packer TODO

## Flipbook support

- [ ] `--frame-size <W>x<H>` — constrain all input images to a fixed frame size for flipbook output. Each input image is treated as one animation frame and padded or placed on a uniform grid.
- [ ] Warn when an input image exceeds the specified frame size; error if `--strict` is also set.
- [ ] Write a `.flipbook.json` descriptor alongside the atlas when `--frame-size` is used, containing `image`, `frame_width`, `frame_height`, `frame_cols`, `frame_rows`, `max_frames`, `fps` (defaulting to 0/unset, overridable via `--fps <N>`), and `loop` (defaulting to true, overridable via `--no-loop`).
- [ ] Frame order matters for flipbooks. Default behaviour should be to sort input images alphabetically (artists typically name frames `001.png`, `002.png`, etc.). Consider also supporting an input metadata file that maps clip names to ordered lists of image paths, which would allow multiple named clips to be packed into a single atlas and described in the descriptor without relying on filename conventions.

## Atlas size control

- [ ] `--max-size <W>x<H>` — set a hard upper bound on the output atlas dimensions instead of always computing the optimal (smallest) size.
- [ ] `--fixed-size <W>x<H>` — force the output atlas to an exact size; error if the images do not fit.
- [ ] Warn when the computed optimal size would exceed a given threshold (useful for catching unexpectedly large atlases in CI).

## Validation

- [ ] Warn when an image appears in multiple input layouts (currently silently deduplicated)
- [ ] `--max-images <N>` — cap the number of images per atlas
