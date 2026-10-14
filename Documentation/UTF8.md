# UTF-8 text support

Unicode text and legacy bitmap symbols are separate. ASCII uses the original
bitmap style. Valid non-ASCII UTF-8 uses the TrueType fallback even when its
code point is below 256: U+00C8 is the letter È, whereas a standalone byte 200
still selects the ammo icon. Drawing, width, height and glyph coverage use the
same selection rule. A missing TrueType glyph uses the atlas question mark with
its correct row and width, rather than the font's .notdef box or an unrelated
bitmap symbol. Valid byte pairs in old symbol strings remain inherently
ambiguous and are interpreted as UTF-8; new text should always be UTF-8.

`UnicodeAtlasFallback` passed device-free content/simulation and two-client
input/UTF-8 checks in 18.16 seconds with 127 ticks. Raw slots 200, 207, 217, 219,
229 matched the source FontLarge PNG pixel for pixel; the Unicode letters with
those scalar values produced different nonempty rasters. U+10FFFF matched `?`
in both pixels and measured width. Cyrillic retained its expected palette color.
`UnicodeAtlasVisual` passed two menu-connected clients, reload HUD and clean
audio shutdown in 57.63 seconds with 65 updates after disconnect. These elapsed
durations describe test runs, not performance comparisons. Native MSBuild
Final|x64 compiled and linked; the root pair passed `UnicodeAtlasDelivered` in
34.90 seconds with two clients, reload HUD, UTF-8 screen text, audio shutdown
and 62 server updates after disconnect. The Meson 35% HUD screenshot was
visually inspected for ammo/reload icons and Cyrillic text. Linux CI
`38527037754` for `a6e09fc2ddf` is pending. The older encoded-index icon comparison below describes
the earlier implementation and is superseded by the direct-atlas regression.

Legacy HUD icons remain compatible with the bitmap atlas: font drawing and
measurement preserve an isolated non-UTF-8 byte as its original unsigned atlas
index. Valid UTF-8 is always decoded as Unicode. This rule belongs only to the
font layer; the strict decoder used for files and network data is unchanged.
Byte sequences that also form valid UTF-8 are necessarily treated as Unicode;
new content should encode glyph code points explicitly. The device-free content
diagnostic checks actual icon pixels and width/height against their UTF-8 encoded
atlas indices, in addition to the Cyrillic raster check. The portable decoder
regression is `Tools/FontGlyphDecodeTest.cpp`.

2026-10-14 validation: `HudIconUTF8` passed the device-free content/simulation
diagnostics and separate two-client probe (128 observed ticks, 18.27 seconds).
`legacy_hud_icon_raster=passed` compares slots 200, 207, 217, 219 and 229 pixel
for pixel; slot 207 intentionally has no visible pixels. Cyrillic retained its
expected palette color. `HudIconUTF8Visual` passed two native clients connecting
through the menu and rendering authoritative reload progress at 35% and 75%
(46.26 seconds, 66 updates after disconnect). Its client-one 35% screenshot was
visually inspected for the restored reload/ammo icons and Cyrillic screen text.
The delivered root `Final|x64` client and matching server also passed
`HudIconUTF8Delivered` (33.95 seconds, two clients, 63 updates after disconnect).
Linux CI `38505177883` for `e8f030257c9` passed GCC font decoding, server build,
device-free self-tests, two separate clients and extracted-package startup.
Build-tree/package runs took 13.34/9.67 seconds and verified UTF-8 config/input/
screen text, no video initialization and clean SIGTERM shutdown. These durations
describe checks, not a performance benchmark or a Linux visual rendering test.

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

On 2026-10-14, the network screen-message check exposed dark TrueType fallback
glyphs next to the yellow bitmap glyphs of FontLarge. Default fallback text now
uses the atlas's most frequent foreground color, excluding its background and
separator; explicit cached font colors remain in use. The original atlas pixels
are unchanged. The assigned Windows client's final screenshot visibly displayed
`Network HUD: Привет, Ёжик!` with readable yellow Cyrillic glyphs. This visual check
covered the built-in large font, not every custom mod font atlas.

Device-free diagnostics now also rasterize `Ж` into an indexed bitmap and check
that actual foreground pixels use the checked-in FontLarge palette index 118.
`UnicodeRasterHeadless` passed on Windows with 54 foreground pixels and no
wrong-color pixels; the whole server/config/two-process check took 19.05 seconds.
This catches the previous dark-glyph regression without a GPU or screenshot.
It is a software raster test, not verification of every graphics driver or mod
font. Changing the bundled atlas's foreground color requires updating this
specific regression expectation.

The delivered Windows Final pair passed the same checks afterward:
`UnicodeRasterDelivered` took 19.44 seconds, verified 54 correctly colored glyph
pixels, two separate clients and 127 post-disconnect ticks.
`UnicodeRasterDeliveredMenu` passed two graphical menu-connected clients in
31.99 seconds, with the Cyrillic fixture applied and 63 further server ticks.
All processes exited cleanly. Linux CI for this additional raster test is still
running; the preceding v19 build and screen-message checks already passed.

Linux verification on 2026-10-14: GitHub Actions run 38453967381 passed UTF-8
decoding, UTF-16 conversion helpers, legacy Windows-1251 fallback, BOM handling,
Cyrillic/emoji filenames through Reader/GUIReader, and ZIP path round trips.
Both the build-tree server and an extracted server package passed Cyrillic
config/log paths and UTF-8 CLI names. Those Linux runs created no GPU context
and do not verify graphical Linux text rendering.

Run `Cortex Command.exe -debug-run 60` (or `CortexCommand -debug-run 60` on Linux) to check UTF-8 decoding, cursor-boundary helpers, supplementary-plane encoding, UTF-16-to-UTF-8 conversion (including unpaired-surrogate replacement), BOM handling, the Windows-1251 fallback, Cyrillic and emoji native-file round trips, UTF-8 opening through `Reader` and `GUIReader`, and visible Cyrillic glyphs in the captured debug screenshot. Windows system error messages use the same UTF-16 bridge before they are passed to the UTF-8 SDL dialog. If the Windows error table supplies replacement characters instead of readable text, the crash report displays the numeric Windows error code instead of mojibake. The run hides its window but still creates an OpenGL context; it is not a GPU-free server mode. This change adds UTF-8 plumbing, not a Russian translation or language selector.
