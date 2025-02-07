cmake_minimum_required(VERSION 3.10)
project(my_project)

set(CMAKE_SYSTEM_NAME WindowsStore)
set(CMAKE_SYSTEM_VERSION 10.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DWINAPI_FAMILY=WINAPI_FAMILY_APP /D_UNICODE /DUNICODE")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)

include_directories(${GLEW_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})
link_directories(${GLEW_LIBRARY_DIRS} ${GLUT_LIBRARY_DIRS})
add_definitions(${GLEW_DEFINITIONS})

add_executable(my_project WIN32
    main.cpp
    # Adicione os outros arquivos de código-fonte aqui
)

target_link_libraries(my_project
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${GLUT_LIBRARIES}
)

set_target_properties(my_project PROPERTIES VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0")
