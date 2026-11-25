# Build beta-01

This tree starts with Red Viper 1.3.4 at
`e28d8f53da32c37fac625368a1bae59d902eaf48`. The inih sources are from
`5e1d9e2625842dddb3f9c086a50f22e4f45dfc2b` and YattLib-3D is from
`5c55013ce1e1e643a30feb1ea67ec765abd5ec54`. They are included in the source
archive. The runtime UI data and its indexed PNG artwork are included too.

Set up devkitARM and the devkitPro 3DS libraries and tools, including
`3ds-zlib`, Citro2D, Citro3D, libctru, tex3ds, picasso, 3dsxtool and
smdhtool. The CIA also needs makerom and bannertool. The beta used devkitARM
GCC 16.1.0, tex3ds 2.3.0 and zlib 1.3.1 on macOS. Check that the host build
tools in your PATH match your host architecture. Work from a path without
spaces as required by the upstream Makefile.

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
make -j6 release TARGET=red-viper-color GIT_HASH=e28d8f5-beta-01 \
  MAKEROM="$(command -v makerom)" BANNERTOOL="$(command -v bannertool)"
```

Run that command inside `emulator/`. It builds `red-viper-color.cia` and
`red-viper-color.3dsx`. Place external game and palette files on SD.

## Host format check

From `emulator/`, with Clang and a POSIX shell:

```sh
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Iinclude tests/check-vbcp.c source/common/vbcp.c -o /tmp/rvc-check-vbcp
/tmp/rvc-check-vbcp ../palettes/*.vbcp
```

The check exercises the production reader against the released pack revision
and rejects a different ROM identity, truncation and oversized record counts.

## VBCP interoperability

The loader reads little-endian `VBGOCP01`, `VBGOCP02` and `VBGOCP03`.
The V3 footer ties the pack to the uncompressed ROM by CRC32 and byte size.
Tile IDs use FNV-1a over each character's 16 raw bytes in Virtual Boy row
order. Each tile record points to 64 row-major RGB triples, with one bit
per pixel indicating where paint applies.

For V3, the loader reads base tiles, layer variants, keep masks, palette
variants and cell variants. A cell is the halfword index in the VIP map area.
The renderer resolves paint for each tile before drawing it and uses a
bounded history of visible markers for context variants. The reference
brightness is in the range 0–63. The renderer handles the context block,
and accepts the ambiguous and explicit right-eye metadata blocks. Its
stereo path uses each eye's own tiles and does not infer left-eye painting
for otherwise unmatched right-eye tiles.

[Upstream palette documentation](https://github.com/yuk27/VBoyColor/blob/9a1cd8f93423b00bdc1aa33c364afc6cfd9c314a/docs/COLOR_PACKS.md)
