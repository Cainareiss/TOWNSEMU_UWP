"""Applies the TSUGARU_UWP branches to the emulator's CMakeLists (idempotent).  Run from anywhere."""
import os, sys

SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src"))

def patch(rel, old, new, marker):
    path = os.path.join(SRC, rel)
    with open(path, "r", newline="") as f:
        text = f.read()
    if marker in text:
        print("already:", rel)
        return
    nl = "\r\n" if "\r\n" in text else "\n"
    old_n = old.replace("\n", nl)
    new_n = new.replace("\n", nl)
    if old_n not in text:
        sys.exit("pattern not found in " + rel)
    text = text.replace(old_n, new_n, 1)
    with open(path, "w", newline="") as f:
        f.write(text)
    print("patched:", rel)

# --- sound -------------------------------------------------------------------------------------------------------
patch("externals/yssimplesound/src/CMakeLists.txt",
"""if(MSVC)
	set(DUMMY_LIBRARY_NAME yssimplesound_dsound)""",
"""if(TSUGARU_UWP)
	set(DUMMY_LIBRARY_NAME yssimplesound_xaudio2)
	set(SUBDIR uwp_xaudio2)
	set(SRCS ${SUBDIR}/yssimplesound_xaudio2.cpp)
	set(LIBS "")
elseif(MSVC)
	set(DUMMY_LIBRARY_NAME yssimplesound_dsound)""",
"TSUGARU_UWP")

patch("externals/yssimplesound/src/CMakeLists.txt",
"""add_subdirectory(unittest)
add_subdirectory(playwav)
""",
"""if(NOT TSUGARU_UWP)
add_subdirectory(unittest)
add_subdirectory(playwav)
endif()
""",
"if(NOT TSUGARU_UWP)")

# --- window connection -------------------------------------------------------------------------------------------
patch("externals/connect_fssimplewindow/CMakeLists.txt",
"""if(MSVC)
	target_link_libraries(${TARGET_NAME} yssimplesound_dsound)""",
"""if(TSUGARU_UWP)
	# The host application draws the picture: GL calls are no-ops (uwp/gl_null.h) and frames are handed over with
	# TsugaruUwpPublishFrame (tsugaru_uwp_host.h).
	target_compile_definitions(${TARGET_NAME} PUBLIC TSUGARU_UWP=1)
	target_compile_options(${TARGET_NAME} PUBLIC /FI${CMAKE_CURRENT_SOURCE_DIR}/uwp/gl_null.h)
	target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../fssimplewindow/src/uwp)
elseif(MSVC)
	target_link_libraries(${TARGET_NAME} yssimplesound_dsound)""",
"TSUGARU_UWP")

# --- fssimplewindow ----------------------------------------------------------------------------------------------
patch("externals/fssimplewindow/src/CMakeLists.txt",
"""if("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsStore")
	set(platform_SRCS
		uwp/fsuwpwrapper.cpp
	)""",
"""if("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsStore")
	set(platform_SRCS
		uwp/fsuwpwrapper.cpp
		windows/fswin32keymap.cpp
	)""",
"windows/fswin32keymap.cpp\n\t)\nelseif(\"${CMAKE_SYSTEM_NAME}\" STREQUAL \"Android\")" if False else "uwp/fsuwpwrapper.cpp\n\t\twindows/fswin32keymap.cpp")

patch("externals/fssimplewindow/src/CMakeLists.txt",
"""target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER ${SUB_FOLDER})

add_library(fssimplenowindow""",
"""target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(TSUGARU_UWP)
	target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/uwp)
	target_compile_definitions(${TARGET_NAME} PUBLIC FSSIMPLEWINDOW_DONT_INCLUDE_OPENGL_HEADERS=1)
endif()
set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER ${SUB_FOLDER})

add_library(fssimplenowindow""",
"FSSIMPLEWINDOW_DONT_INCLUDE_OPENGL_HEADERS=1")

patch("externals/fssimplewindow/src/CMakeLists.txt",
"""add_subdirectory(tests)
""",
"""if(NOT TSUGARU_UWP)
add_subdirectory(tests)
endif()
""",
"if(NOT TSUGARU_UWP)")

# --- gamepad -----------------------------------------------------------------------------------------------------
patch("osdependent/gamepad/CMakeLists.txt",
"""if(MSVC)
	list(APPEND SRCS windows/ysgamepad_windows.c)""",
"""if(TSUGARU_UWP)
	list(APPEND SRCS uwp/ysgamepad_uwp.cpp uwp/ysgamepad_uwp.h)
elseif(MSVC)
	list(APPEND SRCS windows/ysgamepad_windows.c)""",
"TSUGARU_UWP")

patch("osdependent/gamepad/CMakeLists.txt",
"""add_subdirectory(test)""",
"""if(NOT TSUGARU_UWP)
add_subdirectory(test)
endif()""",
"if(NOT TSUGARU_UWP)")

# --- filesys / midi: null back ends -------------------------------------------------------------------------------
patch("osdependent/filesys/CMakeLists.txt",
"""if(MSVC)
	set(SRCS filesys_win.cpp)""",
"""if(TSUGARU_UWP)
	set(SRCS filesys_null.cpp)
elseif(MSVC)
	set(SRCS filesys_win.cpp)""",
"TSUGARU_UWP")

patch("osdependent/midi/CMakeLists.txt",
"""if(MSVC)
	add_library(${TARGET_NAME} midi_interface.h midi_interface.cpp windows/midi_implement.cpp)""",
"""if(TSUGARU_UWP)
	add_library(${TARGET_NAME} midi_interface.h midi_interface.cpp null/midi_implement.cpp)
elseif(MSVC)
	add_library(${TARGET_NAME} midi_interface.h midi_interface.cpp windows/midi_implement.cpp)""",
"TSUGARU_UWP")
