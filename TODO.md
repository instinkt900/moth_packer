# moth_packer TODO

## Padding & fill

- [ ] `--padding <N>` — add N pixels of space around each packed sprite
- [ ] `--fill-color <R,G,B,A>` — colour to use for empty atlas regions (default transparent black)
- [ ] `--padding-mode` — how to fill the padding region around each sprite:
  - `color` — fill with the fill colour (default)
  - `extend` — repeat the outermost edge pixels outward (prevents bilinear bleed)
  - `mirror` — mirror edge pixels into the padding
  - `wrap` — wrap-around edge pixels (useful for tiling textures)

## Packing quality

- [ ] `--sort` — sort images before packing (by area, width, or height) to improve packing efficiency

## Output

- [ ] `--pretty` — pretty-print the JSON descriptor (useful for debugging/inspection)
- [ ] `--absolute-paths` — write atlas image paths as absolute in the JSON (default is relative)
- [ ] `--format <fmt>` — output image format for atlas PNGs (e.g. `png`, `jpg`, `tga`; default `png`)

## Input

- [ ] `-g,--glob <pattern>` — glob pattern input mode (e.g. `"assets/**/*.png"`) as an alternative to a txt file or directory
- [ ] Support `.webp` input — stb_image supports it via `STBI_WEBP`

## Validation

- [ ] Warn when an image appears in multiple input layouts (currently silently deduplicated)
- [ ] `--max-images <N>` — cap the number of images per atlas
