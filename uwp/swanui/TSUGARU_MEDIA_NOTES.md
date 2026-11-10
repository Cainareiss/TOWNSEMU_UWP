# Tsugaru (FM Towns) UWP port - media and wording changes to the frontend copy

All edits to the frontend copy in `uwp\swanui` are under `#if defined(TSUGARU_UWP)` (or new files), so the PS1 behaviour
is still there with the macro off. Build both C and C++ with `/DTSUGARU_UWP=1 /DSWANUI_XBOX=1`. The bridge C++ files need
`/std:c++20` (folder_uwp.cpp uses coroutines).

## Media: CD or floppy

New files
- `compat\swanui_towns_media.h` / `.c`: `towns_media_of_path(path)` -> `TOWNS_MEDIA_CD` (0) or `TOWNS_MEDIA_FD` (1);
  `towns_companion_fd(game_path, out, size)`; `towns_serial_from_title(title, out, size)`.
  - FD: `.d77`, `.d88`, or a raw `.bin`/`.img` of at most 2 MB (looked at with stat). CD: everything else (`.cue`,
    `.mds`, `.iso`, `.mdf`, bigger raw images). A `.m3u` has the media of the first image it lists.
  - Companion floppy: a CD game may come with a floppy beside it (Towns CD games often want one in drive 0): the image
    whose name starts with the CD image's name, else the only floppy image of a folder that holds a single CD game.

Changed
- `src\library.h`: `Game.media` (new int, TSUGARU_UWP only).
- `src\library.c`: `image_rank` (cue/mds 3; iso/img/mdf/d77/d88 2; bin 1; m3u 6; no chd/pbp/ccd/ecm), `extract_serial`
  (no PS1 prefixes), `core_serial` (no core to ask), `finish_game` (serial = `FMT-XXXXXXXX` from a hash of the cleaned
  title, id = serial, `media` set), `del_collect` (deleting a game removes the `.mdf` of an `.mds`; no `.sbi`).
- `src\disc.c`: `disc_read_serial` and `disc_serial_from_reader` return false at once (no SYSTEM.CNF on a Towns disc).
- `compat\swanui_xbox_library.c`: no change needed (it hides what a sheet refers to; a raw floppy image is not referenced).

What the host does (host_load gets only the path and serial)
- `towns_media_of_path(game_path)` decides `-CD <path>` or `-FD0 <path>`.
- For a CD game call `towns_companion_fd(game_path, buf, size)`: when it returns 1 add `-FD0 <buf>`.
- A `.m3u` (several discs of a game, written by the library for "(Disc n)" / "(CD n)" files) is not understood by Tsugaru:
  host_load must open its first line, and `host_disc_select` swap to the other lines.
- The library does not touch the files; the old `swanstation_retro_ds` symbol is no longer referenced.

Known gaps
- A loose raw floppy `.bin` lying in a folder that also has `.cue` sheets is hidden (as a PS1 `.bin` would be): use `.img`,
  `.d77` or `.d88` there. In a game folder it is fine.
- Two floppies in one folder (disk A and B): only the first is picked as companion; the library groups "(Disc n)" ones
  in a `.m3u` of floppies, which the host must handle like the CD playlist.
- The 3D case and the shelf's other PS1 wording that is not in the override tables (tips, `game-kinds.txt`) is data keyed
  by PS1 titles: it never matches a Towns game, so it never shows.

## Settings screen (rows hidden, not deleted)

`src\ui\settings_screen.c`: `towns_hidden()` joins the existing `xbox_hidden()` filter: every SwanStation core option,
"Start BIOS", "Memory cards", Renderer, Internal resolution, PGXP, True colour, Supersampling, Load HD textures,
Controller (DualShock type), Players (multitap), NeGcon, Light gun, PS1 startup intro, Known game fixes, Fast CD loading,
Region. `src\ui\coverflow.c`: the shelf button that opened the memory cards does nothing.

## Wording (compat\swanui_xbox_i18n.c)

`TOWNS_TEXTS[]` is consulted before `XBOX_TEXTS[]`: BIOS -> "ROM files", the empty-shelf lines (supported extensions: CD
and floppy images; "ROM: ... found" / "ROM: put your FM Towns ROM files (FMT_SYS.ROM...) in a folder named rom inside the
games folder"), the "BIOS not found" toast, PlayStation Mouse -> mouse, "tap = Select", the About credits (Tsugaru by
Soji Yamakawa, BSD-3-Clause) and the licences line (FM TOWNS is Fujitsu's trademark). The host's `host_bios_present()`
should therefore report whether the FM Towns ROM set was found (a folder named `rom` in the games folder, or whatever the
host decides: then change those three strings).

## Art (tools\make_towns_art.ps1, originals in tools\ps1_art_orig)

`assets\psx_disc.png` (same 1024x1024): plain "FM TOWNS CD-ROM" label, no logos. `psx_case_front.png`, `psx_case_back.jpg`:
Hypnagogia/NTSC/PAL/SLUS spine and the PS1 demo advert replaced by "FM TOWNS CD-ROM". `nocover2.png`: PlayStation logo and
wordmark -> FM TOWNS. `nocover-flamenco.png`: "A PSI EMULATOR" -> "AN FM TOWNS EMULATOR". The file names are unchanged, so no
code refers to new names. The credits string for the jewel case's author (CC BY 4.0, sodaraptor) is still in the About text.

## Network

`net_uwp.cpp` keeps `net_available()` false unless `<LocalState>\enable-net.txt` exists: every download the frontend
starts (covers by PS1 serial, libretro-thumbnails, bezels, cheats, game database) points at PlayStation repositories, and
there is no FM Towns equivalent (checked: no libretro-thumbnails repo for it). Covers are found locally (cover.png beside
the game, `covers\<title>.png`, picked art) or a title card is drawn.

## Bridge files (uwp\bridge)

`uwp_paths.h/.cpp` (data and install roots, `uwp_log`), `net_uwp.cpp` (Windows.Web.Http), `folder_uwp.cpp` (folder picker +
FutureAccessList, token `tsugaru-games`), `storage_uwp.cpp` (drive letters), `ui_uwp.cpp`, `gamedb_uwp.cpp` (empty
database), `ra_uwp.cpp` (no-ops, includes `ra_password_login`), `uwp_stubs.cpp` (NFS/NTFS/disc import/updater/remote/Vulkan
probe stubs), `swanui_cdecl.h`. Link `WindowsApp.lib`. Also compile `swanui\third_party\rcheevos\src\rhash\md5.c`
(used by `core\bios_choices.c`; the DuckStation build got it from its own copy).

Check scripts: `check_cpp.cmd` (one bridge file), `check_c.cmd` (one frontend C file), `check_all_c.cmd` (all frontend C
files, needs `E:\claude-work\tmp\fe_list.txt`), `tools\symbol_audit.ps1` (unresolved externals).
