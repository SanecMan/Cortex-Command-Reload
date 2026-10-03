# UTF-8 text support

The client now treats text passed through the GUI as UTF-8. Existing bitmap fonts continue to render their original atlas characters; code points outside that atlas use the bundled Roboto Medium font in `Data/Base.rte/GUIS/Fonts/`. This keeps old `.rte` font assets usable while allowing Unicode glyphs where text is supplied as UTF-8.

Text fields move, select, delete, and enforce their length limit by Unicode code point instead of splitting UTF-8 byte sequences. The data and GUI readers skip a UTF-8 byte-order mark when one is present, and Windows file paths passed to those readers use `std::filesystem::u8path`.

MSVC and Meson builds compile C++ source as UTF-8. Runtime `.ini`, Lua, and mod data are not silently transcoded: files containing text should be saved as UTF-8. Existing ASCII content remains unchanged. No Russian translation or language selector is included in this change.

The automated `-debug-run` checks UTF-8 decoding (including malformed input), BOM handling, and includes a visible `UTF-8 test: Привет, Ёжик!` line in its captured gameplay screenshots. The debug run still creates an OpenGL context; it hides the window but is not a GPU-free/headless run.
