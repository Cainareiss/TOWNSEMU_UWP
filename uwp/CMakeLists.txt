cmake_minimum_required(VERSION 3.21)
project(TSUGARU_UWP C CXX)

# Tsugaru (FM TOWNS emulator) for UWP / Xbox.  Configure with:
#   cmake -S uwp -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.26100.0
# and build the target  tsugaru_uwp  (see uwp/tools/build.ps1).

set(TSUGARU_UWP ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(DISC_IMAGE_DIR "C:/D/TownsISO")

add_compile_definitions(YS_LITTLE_ENDIAN YS_TWOS_COMPLEMENT TSUGARU_UWP=1 UNICODE _UNICODE NOMINMAX
	_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS)
add_compile_options(/utf-8 /EHsc /MP /wd4244 /wd4267 /wd4996 /wd4018)

enable_testing()

function(tsugaru_dir rel)
	add_subdirectory(${SRC}/${rel} ${CMAKE_BINARY_DIR}/core/${rel})
endfunction()

tsugaru_dir(resources)
tsugaru_dir(osdependent)
tsugaru_dir(vgmrecorder)
tsugaru_dir(cpputil)
tsugaru_dir(lineparser)
tsugaru_dir(miscutil)
tsugaru_dir(cheapmath)
tsugaru_dir(vmbase)
tsugaru_dir(device)
tsugaru_dir(cpu)
tsugaru_dir(inout)
tsugaru_dir(ramrom)
tsugaru_dir(ym2612)
tsugaru_dir(rf5c68)
tsugaru_dir(i8251)
tsugaru_dir(i8251_to_socket)
tsugaru_dir(i8253)
tsugaru_dir(diskdrive)
tsugaru_dir(customstick)
tsugaru_dir(towns)
tsugaru_dir(discimg)
tsugaru_dir(diskimg)
tsugaru_dir(externals)
tsugaru_dir(main_cui/command)
tsugaru_dir(main_cui/argv)

# The emulator's own main() (patched: TsugaruUwpMain, no console) as a library.
add_library(tsugaru_emu STATIC ${SRC}/main_cui/main.cpp)
target_link_libraries(tsugaru_emu
	towns
	townscommand
	townsargv
	townsrender
	outside_world
	fssimplewindow_connection
)

# d77 reads images through the brokered stream header that lives in cpputil.
target_include_directories(d77 PRIVATE ${SRC}/cpputil)

################################################################################
# The Flamenco One frontend (plain C, GPL-3.0): the shelf, menus and settings.  Only src/platform/platform.h and
# src/core/host.h are implemented by us (bridge/plat_d3d11.cpp, bridge/host_tsugaru.cpp).
################################################################################
set(SW ${CMAKE_CURRENT_SOURCE_DIR}/swanui)
set(SWANUI_FILES
	src/art.c src/bezels.c src/cheats.c src/config.c src/controls.c src/covers.c src/disc.c src/i18n.c src/library.c
	src/main.c src/play.c src/profiles.c src/stats.c src/stb_impl.c src/tips.c src/util.c src/vendor.c
	src/core/bios_choices.c src/core/swanstation_options.c
	src/ui/case3d.c src/ui/cheat_search.c src/ui/coverflow.c src/ui/draw.c src/ui/guide_screen.c src/ui/icons.c
	src/ui/login_screen.c src/ui/manual_screen.c src/ui/memcards_screen.c src/ui/menu_screen.c
	src/ui/profile_screen.c src/ui/settings_screen.c src/ui/sfx.c src/ui/stats_screen.c src/ui/text.c
	src/ui/textinput_screen.c src/ui/theme.c
	src/platform/xbr.c src/platform/blit.c third_party/rcheevos/src/rhash/md5.c
	compat/swanui_compat.c compat/swanui_xbox_i18n.c compat/swanui_xbox_folder.c compat/swanui_xbox_library.c compat/swanui_towns_media.c compat/swanui_towns_art.c
)
list(TRANSFORM SWANUI_FILES PREPEND ${SW}/)
add_library(swanui STATIC ${SWANUI_FILES})
target_compile_definitions(swanui PRIVATE SWANUI_XBOX=1 _UWP _LIB)
target_include_directories(swanui PRIVATE
	${SW}/compat ${SW}/src ${SW}/third_party/stb
	${SW}/third_party/swanstation/dep/libretro-common/include ${SW}/third_party/rcheevos/include)
target_compile_options(swanui PRIVATE /FI${SW}/compat/swanui_compat.h /std:c17
	/wd4100 /wd4146 /wd4018 /wd4244 /wd4267 /wd4996 /wd4068)

################################################################################
# The application
################################################################################
set(BR ${CMAKE_CURRENT_SOURCE_DIR}/bridge)
add_executable(tsugaru_uwp WIN32
	app/uwp_app.cpp
	${BR}/plat_d3d11.cpp
	${BR}/host_tsugaru.cpp
	${BR}/host_tsugaru_vm.cpp
	${BR}/net_uwp.cpp
	${BR}/folder_uwp.cpp
	${BR}/storage_uwp.cpp
	${BR}/ui_uwp.cpp
	${BR}/gamedb_uwp.cpp
	${BR}/ra_uwp.cpp
	${BR}/uwp_paths.cpp
	${BR}/uwp_stubs.cpp
)
target_include_directories(tsugaru_uwp PRIVATE
	${BR} ${SW}
	${SRC}/externals/fssimplewindow/src/uwp
	${SRC}/osdependent/gamepad/uwp
	${SRC}/osdependent/gamepad)
target_compile_definitions(tsugaru_uwp PRIVATE SWANUI_XBOX=1 _UWP)
target_link_libraries(tsugaru_uwp PRIVATE
	swanui tsugaru_emu ysgamepad towns townscdrom discimg
	d3d11 dxgi xaudio2 xinputuap WindowsApp ws2_32 runtimeobject)
# The package is made by uwp/tools/package.ps1 (own manifest, own certificate); skip MSBuild's packaging step.
set_target_properties(tsugaru_uwp PROPERTIES
	VS_GLOBAL_GenerateAppxPackageOnBuild false
	VS_GLOBAL_AppxPackage false
	VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION 10.0.17134.0)
# C++/WinRT coroutines (folder picker) need C++20.
set_target_properties(tsugaru_uwp PROPERTIES CXX_STANDARD 20)
