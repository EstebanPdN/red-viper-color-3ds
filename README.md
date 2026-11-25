# Red Viper Color for Nintendo 3DS

I’m adapting Red Viper to play Virtual Boy games in color on the 3DS. Full Color combines hand-painted tiles with automatic colors and renders each eye separately.

[**Download beta-01**](https://github.com/EstebanPdN/red-viper-color-3ds/releases/tag/beta-01)

## What you need

- A Nintendo 3DS or 2DS family system with Luma3DS. The CIA uses FBI. The 3DSX runs from the Homebrew Launcher. For audio, set up your console’s own DSP firmware.
- Your own Virtual Boy homebrew or a game you have dumped. Put your files in `/vb/` at the root of the SD card.

Copy the 3DSX to `/3ds/red-viper-color/` for the Homebrew Launcher. The CIA shares Title ID `000400000FE7CB00` with Red Viper and replaces the original HOME Menu installation.

## ROMs and color packs

Choose **Choose ROM** from the lower screen. On first use, the browser opens `sdmc:/vb/` when that folder exists, then tries `sdmc:/red-viper/`, then the SD root. It returns to the last ROM folder on later launches. The browser lists files and folders without displaying the full path.

For a sidecar palette, give the ROM and the palette the same base name. For example, place `My Game.vb` and `My Game.vbcp` together in `/vb/`. The palette must match the ROM’s size and CRC32. For a ZIP, name the palette after the ZIP and place it beside the archive. In **Settings → Image and Color**, select **Color Mode: Full Color**.

The emulator checks the sidecar first, then `sdmc:/red-viper-color/colorpacks/XXXXXXXX.vbcp` and `sdmc:/red-viper/colorpacks/XXXXXXXX.vbcp`. Here `XXXXXXXX` is the ROM CRC32 in uppercase hex. A missing palette lets the search continue. If no compatible pack loads, Full Color uses automatic colors. Version 3 packs need a matching ROM footer. Battery saves use a `.ram` file beside the selected ROM.

The [SD palette ZIP](https://github.com/EstebanPdN/red-viper-color-3ds/releases/download/beta-01/Red-Viper-Color-SD-Palettes.zip) contains 16 hand-painted palettes. Copy its `red-viper-color` folder to the root of your SD card. The palettes come from [VBoyColor by yuk27](https://github.com/yuk27/VBoyColor/tree/9a1cd8f93423b00bdc1aa33c364afc6cfd9c314a/colorpacks). I checked every file against that revision. Each file holds color rules tied to a particular game revision. The [format guide](https://github.com/yuk27/VBoyColor/blob/9a1cd8f93423b00bdc1aa33c364afc6cfd9c314a/docs/COLOR_PACKS.md) explains how the palettes are made and shared. [Palette checksums](palettes/SHA256SUMS.txt) · [Upstream license](palettes/LICENSE.txt)

## Beta notes

I checked Full Color with both download formats in Azahar's New and Old 3DS
modes. Tests on real consoles are still pending. This beta uses the original
renderer for anaglyph and local multiplayer. Full Color can run slower in
complex scenes, and some eye-specific or context-dependent tiles can use
different colors. Switch to Single Color in **Settings → Image and Color**
if you run into a rendering issue.

[Source and build instructions](emulator/BUILDING.md) · [Licenses and credits](emulator/LICENSES.md)

## Beta feedback

If you run into a problem, open an issue with your console model or emulator version, launcher, build, game revision or CRC32, color setting, and the steps that reproduced it. For a palette problem, include the name of the palette and the part of the game where the colors change. Keep ROMs, firmware, saves and memory dumps on your own device. [Report an issue](https://github.com/EstebanPdN/red-viper-color-3ds/issues)

## Credits

My color adaptation builds on [Red Viper 1.3.4 by skyfloogle and contributors](https://github.com/skyfloogle/red-viper/tree/e28d8f53da32c37fac625368a1bae59d902eaf48), r3Ddragon, Reality Boy by David Tucker and Red Dragon. The palette format and painted color packs come from [VBoyColor by yuk27](https://github.com/yuk27/VBoyColor/tree/4567a05845449895e2b3a8ca79e45b49bb1ffbcb).

This project was developed with the help of [OpenAI Codex](https://openai.com/codex/) and [Claude Code](https://claude.com/product/claude-code). Learn more about the development process [here](AI_DEVELOPMENT.md).

[Join Esteban's Forge on Discord](https://discord.com/servers/esteban-s-forge-1533248895663210506)
