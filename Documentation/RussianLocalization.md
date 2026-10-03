# Russian localization

The built-in interface language is selected in **Settings → Misc → Language**. The selection is saved in `Userdata/Settings.ini` and applies immediately to text drawn with the built-in bitmap fonts. English remains the fallback language.

Translations live in `Data/Base.rte/Languages/ru.tsv`. Each UTF-8 line contains the original English text, a tab, and its Russian translation. Use `\\n` for a line break. Keep English source text unchanged when possible: this lets existing modules and Lua content continue to use their original strings.

Lua scripts can request a translated string with:

```lua
local text = Localization.Get("English source text")
```

If no Russian entry exists, the API returns the English input. New Cyrillic glyphs are supplied by the `_ru.bmp` bitmap-font atlases in the built-in GUI skin directories. The game converts catalog UTF-8 text to the legacy Windows-1251 character slots only at the bitmap-font boundary.
