# UTF-8 text support

Absolute resource paths remain absolute when resolved by PresetMan. They are
normalized to generic separators and passed through the same UTF-8/legacy-path
conversion as relative module paths, without adding `Mods/`. Relative `.rte`
paths retain their existing Data/Mods/userdata routing. The 2026-10-08 diagnostic
run `ReplicaCallbacks_Ёжик` loaded a generated Lua script by an absolute path
with a Cyrillic directory name, executed its native callback control, removed
the temporary script and passed with exit code 0. This addresses the observed
failure where the Lua loader prefixed an absolute Windows script path with Mods/.

The engine's text interfaces use UTF-8 bytes. `Reader` and the GUI reader skip an optional UTF-8 BOM at the start of a text file. Valid UTF-8 values remain unchanged; malformed non-UTF-8 text is interpreted as Windows-1251 for compatibility with older `.rte` content. This is a best-effort legacy fallback because arbitrary byte sequences cannot always be distinguished from valid UTF-8. File paths exposed by the engine are UTF-8: use `UTF8::PathFromString`/`UTF8::PathToString` for filesystem paths and `UTF8::OpenFile` for C `FILE*` access. The path conversion uses the C++20 `char8_t` API and passes strict GCC 13 compilation without deprecated `u8path` calls. These helpers translate to native wide paths on Windows; the main Reader, GUI Reader, `Writer`, GUI Writer, Lua file APIs, image metadata reader, module extractor, and case-sensitive path check use UTF-8-aware paths.

GUI text walks Unicode code points for drawing, measurement and editing. Text fields move, select, delete and enforce their length limit by code point instead of splitting UTF-8 byte sequences. Existing bitmap atlases continue to draw the code points they contain. Other glyphs use the bundled `Data/Base.rte/GUIS/Fonts/Roboto-Medium.ttf`, rasterized through the existing TrueType helper; if the font cannot be loaded or lacks a glyph, the UI falls back to `?`. The game still uses its original bitmap fonts for the existing glyphs and their visual style. MSVC and Meson builds compile C++ source as UTF-8.

`Reader` converts non-module paths and module names through the UTF-8 filesystem helpers before opening files. `GUIReader` uses the same conversion for GUI definitions and included files. Valid UTF-8 paths stay unchanged; malformed legacy Windows-1251 path bytes are converted before filesystem access so older Cyrillic module and GUI filenames remain usable. ZIP archives used for module extraction and saved games are opened through minizip-ng's native OS stream, which treats its path argument as UTF-8 and uses wide Windows file APIs. The automated debug run writes a ZIP under a Cyrillic filename, reopens and reads it, then removes it; interrupted test archives are named with the debug-run prefix so the next run cleans them.

For new `.rte` and Lua text, save files as UTF-8. Existing Windows-1251 text is kept readable when it is not valid UTF-8, so old mods do not need to be mass-converted. The engine's Lua file loader preserves valid UTF-8, strips a UTF-8 BOM, accepts shebang lines and converts malformed legacy Windows-1251 source before compiling it. Recognized Lua/LuaJIT bytecode bypasses text conversion. `loadfile`, `dofile` and the added Lua module searcher use this loader and UTF-8 paths; native/C module searchers remain available. Lua strings constructed at runtime are byte strings and are not automatically transcoded: pass UTF-8 strings to GUI APIs.

Windows verification on 2026-10-13: `EmitterV17UTF8` exited 0 after 60 updates.
It passed UTF-8 and legacy source loading, BOM handling, `loadfile`, `dofile`,
`require`, Cyrillic filesystem operations and glyph rendering. The native file
and ZIP path checks also passed. These checks exercise the engine-managed Lua
file paths; they do not establish Unicode-aware behavior for Lua's standard
byte-indexed string operations or arbitrary native modules.

Run `Cortex Command.exe -debug-run 60` (or `CortexCommand -debug-run 60` on Linux) to check UTF-8 decoding, cursor-boundary helpers, supplementary-plane encoding, UTF-16-to-UTF-8 conversion (including unpaired-surrogate replacement), BOM handling, the Windows-1251 fallback, Cyrillic and emoji native-file round trips, UTF-8 opening through `Reader` and `GUIReader`, and visible Cyrillic glyphs in the captured debug screenshot. Windows system error messages use the same UTF-16 bridge before they are passed to the UTF-8 SDL dialog. If the Windows error table supplies replacement characters instead of readable text, the crash report displays the numeric Windows error code instead of mojibake. The run hides its window but still creates an OpenGL context; it is not a GPU-free server mode. This change adds UTF-8 plumbing, not a Russian translation or language selector.
