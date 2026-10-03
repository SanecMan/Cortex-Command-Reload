# UTF-8 text support

The engine's text interfaces use UTF-8 bytes. `Reader` skips an optional UTF-8 BOM at the start of a text file. Valid UTF-8 values remain unchanged; malformed non-UTF-8 text is interpreted as Windows-1251 for compatibility with older `.rte` content. This is a best-effort legacy fallback because arbitrary byte sequences cannot always be distinguished from valid UTF-8.

GUI text walks Unicode code points for drawing, measurement and editing. Existing bitmap atlases continue to draw the code points they contain. Other glyphs use the bundled `Data/Base.rte/GUIS/Fonts/Roboto-Medium.ttf`, rasterized through the existing TrueType helper; if the font cannot be loaded or lacks a glyph, the UI falls back to `?`. The game still uses its original bitmap fonts for the existing glyphs and their visual style.

For new `.rte` text, save files as UTF-8. Existing Windows-1251 text is kept readable when it is not valid UTF-8, so old mods do not need to be mass-converted. Lua source and string literals are not automatically transcoded; save new Lua files as UTF-8 and pass UTF-8 strings to GUI APIs.

Run `Cortex Command.exe -debug-run 60` (or `CortexCommand -debug-run 60` on Linux) to check UTF-8 decoding, cursor-boundary helpers, supplementary-plane encoding, BOM handling, the Windows-1251 fallback, and visible Cyrillic glyphs in the captured debug screenshot.
