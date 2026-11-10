# Tsugaru (FM Towns) UWP port - branding and theme changes to the frontend copy (fork B2, 2026-11-03)

Product name on screen: **Tsugaru**. Everything under `#if defined(TSUGARU_UWP)` or in the Xbox/Towns text tables.
Compile check: `uwp\check_all_c.cmd` (43 files, 0 errors).

## compat\swanui_xbox_i18n.c
- `BRAND_TO` = "Tsugaru": `ui/text.c` passes every drawn string through `swanui_xbox_brand()`, so "SwanStationPS5",
  "SwanStation" and "PSXS5" in REF literals (i18n.c, toasts, help) read "Tsugaru".
- `brand_needed()`: strings that contain "darkxex", "SynoPiia" or "THIRD_PARTY" are left as they are, so the About
  credits can name the original frontend (SwanStationPS5 by darkxex) honestly.
- `XBOX_TEXTS`: every "Flamenco One" -> "Tsugaru" (en + pt); every `SwanStation\...` / `LocalState\SwanStation` path
  hint -> `Tsugaru\...` (the data root is now `<LocalState>\Tsugaru`, plat_default_root in the D3D11 layer).
- `TOWNS_TEXTS` (consulted first): ROM rows rewritten for the built-in ROMs (About > BIOS row "Built in: ... your own set
  in LocalState\Tsugaru\bios, or rom inside the games folder, is used instead"; empty-shelf "ROM: the FM Towns system
  ROMs are built in." / "ROM: none found..."; the "BIOS not found" toast). New rows: About > Version help, This build
  (GPL + BSD: may be passed on with its source), Created by help, Interface credits (based on SwanStationPS5 by
  darkxex), Thanks (Kasanova/UNZ, Free FM TOWNS Project, jewel case, fonts, icons; no PS1 cover sources), USB folder
  help (fmtowns instead of psx), empty-shelf "in a Tsugaru or fmtowns folder", covers texts (not downloaded: cover.png
  or <title>.png beside the game, or LocalState\Tsugaru\covers), bezel help, "Scan for the interface's GitHub page".

## src\ui\settings_screen.c
- `XB_INTERFACE` = "SwanStationPS5 1.6.0 by darkxex" (TSUGARU_UWP; the brand rewrite skips it because of "darkxex").
- ABOUT group caption "Flamenco One" -> "Tsugaru" (the help key unchanged, overridden in TOWNS_TEXTS).
- `towns_hidden()`: + "Download missing covers" (no FM Towns cover repository).
- `SP_CREDITS` value: "Soji Yamakawa (CaptainYS)" (TSUGARU_UWP); the interface author is in the row's help.
- About QR: the QR image still encodes github.com/darkxex/SwanStationPS5 (qr-github.png; no QR generator here); the
  second line now reads "Emulator: https://github.com/captainys/TOWNSEMU" (TSUGARU_UWP).

## src\ui\theme.h / theme.c / src\config.c
- New theme `THEME_MARTY` ("Marty", TSUGARU_UWP only, last in the enum so saved theme numbers keep their meaning) and
  the default theme in `config_defaults` (`s->theme = THEME_MARTY`). Palette:
  bg #F6F6F6 (cream), bg_deep #E6EDF6, card #FFFFFF, row_selected #D7E6F8, focus #2F6FBF (logo blue), pill #E4EAF2,
  switch_on #3B87D1, switch_off #C3CBD6, text #1C2A3C, text_dim #56687E, text_soft #30425A, gold #C0961A (star
  yellow darkened), danger #D14F4D (salmon darkened), good #2A9A72 (mint darkened), divider #D3DCE8, cover_outline
  #3B87D1; light = true, LAYOUT_GRID on BACKDROP_FLAT (the Memory Card combination), fonts Inter-400 / ChakraPetch-700.
- `usb_folder` default "fmtowns" instead of "psx" (TSUGARU_UWP).

## src\main.c
- The two "BIOS not found, SCPH1001.BIN is recommended..." toasts call `tr()` with the S3 key, so the ROM text applies.

## Assets (tools\make_marty_art.py, from uwp\package\src\marty_icon.png; file names unchanged)
- `assets\flamenco-emblem.png` 512x512 RGBA: the Marty mascot + "FM TOWNS MARTY" word mark, backdrop keyed to alpha.
- `assets\nocover-flamenco.png` 500x500 RGBA: "TSUGARU / FM TOWNS EMULATOR / NO COVER" card with the mascot and a
  blue-salmon-mint-yellow stripe.

## Comments only
- compat\swanui_xbox_folder.c/.h, swanui_xbox_library.c/.h headers: "Flamenco One (SwanStation Xbox)" -> Tsugaru.

## Not renamed (on purpose)
- Code identifiers and macros (`SwanStationPS5_log`, `SwanStationPS5_PATH_MAX`, `SWANUI_XBOX`...), the compat file
  names, `SwanStationPS5_NAME` (User-Agent) and the GPL headers: not user-visible.
- The data folder is `swanui\assets` inside the package: the host's asset path, not shown to the user.
- `assets\game-tips.txt` lines name SwanStationPS5 (PS1 titles only, never match an FM Towns game; the brand rewrite
  would show "Tsugaru" anyway).

## Also (after the first check)
- `src\SwanStationPS5.h:19`: `SwanStationPS5_TITLE` "Flamenco One" -> "Tsugaru" (the shelf title, coverflow.c:837/1009, main.c:912).
- `src\ui\coverflow.c:2232`: the empty-shelf key is now `"BIOS: included in Tsugaru."` (the table keys were renamed with the brand).
- Comments in `src\ui\text.c`, `compat\swanui_xbox_ui.h`.
