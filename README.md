# Red Viper Color for Nintendo 3DS

I’m adapting Red Viper to play Virtual Boy games in color on the 3DS. Full Color combines hand-painted tiles with automatic coloring and keeps a separate view for each eye.

**Beta-01 status.** The palettes and setup notes are available here. The CIA and 3DSX are pending redistribution clearance for the combined emulator.

## What you need

- A Nintendo 3DS or 2DS family system with Luma3DS. The CIA uses FBI. The 3DSX runs from the Homebrew Launcher. For audio, set up your console’s own DSP firmware.
- Your own Virtual Boy homebrew or a game you have dumped. Put your files in `/vb/` at the root of the SD card.

If you already have a test build, copy the 3DSX to `/3ds/red-viper-color/` for the Homebrew Launcher. The CIA shares Title ID `000400000FE7CB00` with Red Viper and replaces the original HOME Menu installation.

## ROMs and color packs

Choose **Choose ROM** from the lower screen. On first use, the browser opens `sdmc:/vb/` when that folder exists, then tries `sdmc:/red-viper/`, then the SD root. It returns to the last ROM folder on later launches. The browser lists files and folders without displaying the full path.

For a sidecar palette, give the ROM and the palette the same base name. For example, place `My Game.vb` and `My Game.vbcp` together in `/vb/`. The palette must match the ROM’s size and CRC32. For a ZIP, name the palette after the ZIP and place it beside the archive. In **Settings → Image and Color**, select **Color Mode: Full Color**.

The emulator checks the sidecar first, then `sdmc:/red-viper-color/colorpacks/XXXXXXXX.vbcp`, `sdmc:/red-viper/colorpacks/XXXXXXXX.vbcp`, and the built-in copy. Here `XXXXXXXX` is the ROM CRC32 in uppercase hex. A missing palette lets the search continue. An incompatible or damaged palette stops the search and leaves Auto coloring available. Battery saves use a `.ram` file beside the selected ROM.

The [16 hand-painted palettes](palettes/) come from [VBoyColor by yuk27](https://github.com/yuk27/VBoyColor/tree/9a1cd8f93423b00bdc1aa33c364afc6cfd9c314a/colorpacks). I checked every file against that revision. Each file holds color rules tied to a particular game revision. The [format guide](https://github.com/yuk27/VBoyColor/blob/9a1cd8f93423b00bdc1aa33c364afc6cfd9c314a/docs/COLOR_PACKS.md) explains how the palettes are made and shared. [Palette checksums](palettes/SHA256SUMS.txt) · [Upstream license](palettes/LICENSE.txt)

## Beta feedback

If you run into a problem, open an issue with your console model or emulator version, launcher, build, game revision or CRC32, color setting, and the steps that reproduced it. For a palette problem, include the name of the palette and the part of the game where the colors change. Keep ROMs, firmware, saves and memory dumps on your own device. [Report an issue](https://github.com/EstebanPdN/red-viper-color-3ds/issues)

## Credits

My color adaptation builds on [Red Viper 1.3.4 by skyfloogle and contributors](https://github.com/skyfloogle/red-viper/tree/e28d8f53da32c37fac625368a1bae59d902eaf48), r3Ddragon, Reality Boy by David Tucker and Red Dragon. The automatic color and depth work draws on [VBoyColor by yuk27](https://github.com/yuk27/VBoyColor/tree/4567a05845449895e2b3a8ca79e45b49bb1ffbcb).

This project was developed with the help of [OpenAI Codex](https://openai.com/codex/) and [Claude Code](https://claude.com/product/claude-code). Learn more about the development process [here](AI_DEVELOPMENT.md).
