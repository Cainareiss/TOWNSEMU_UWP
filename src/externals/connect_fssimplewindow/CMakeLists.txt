set(TARGET_NAME fssimplewindow_connection)

add_library(${TARGET_NAME} fssimplewindow_connection.h fssimplewindow_connection.cpp)
target_link_libraries(${TARGET_NAME} outside_world fssimplewindow yssimplesound cpputil ysgamepad)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(TSUGARU_UWP)
	# The host application draws the picture: GL calls are no-ops (uwp/gl_null.h) and frames are handed over with
	# TsugaruUwpPublishFrame (tsugaru_uwp_host.h).
	target_compile_definitions(${TARGET_NAME} PUBLIC TSUGARU_UWP=1)
	target_compile_options(${TARGET_NAME} PUBLIC /FI${CMAKE_CURRENT_SOURCE_DIR}/uwp/gl_null.h)
	target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../fssimplewindow/src/uwp)
elseif(MSVC)
	target_link_libraries(${TARGET_NAME} yssimplesound_dsound)
elseif(APPLE)
	find_library(COCOA_LIB Cocoa)
	mark_as_advanced(COCOA_LIB)
	target_link_libraries(${TARGET_NAME} yssimplesound_macosx ${COCOA_LIB})
elseif(UNIX)
	target_link_libraries(${TARGET_NAME} yssimplesound_linux_alsa)
endif()
