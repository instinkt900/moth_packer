# moth_packer TODO

## Output

- [ ] `--format <fmt>` — output image format for atlas PNGs (e.g. `png`, `jpg`, `tga`; default `png`)

## Input

- [ ] Support `.webp` input — stb_image supports it via `STBI_WEBP`

## Validation

- [ ] Warn when an image appears in multiple input layouts (currently silently deduplicated)
- [ ] `--max-images <N>` — cap the number of images per atlas
