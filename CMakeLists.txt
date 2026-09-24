cmake_minimum_required(VERSION 4.4)

project(pvz-cheats LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Injected dll containing the actual cheats.
add_library(pvz-cheats SHARED
    cheats/main.cpp
)
target_include_directories(pvz-cheats PRIVATE common)

# Win32 UI that injects the dll and forwards commands over a named pipe.
add_executable(pvz-cheats-injector WIN32
    injector/channel.cpp
    injector/cheat.cpp
    injector/inject.cpp
    injector/main.cpp
    injector/process.cpp
    injector/ui.cpp
)
target_include_directories(pvz-cheats-injector PRIVATE common)
target_link_libraries(pvz-cheats-injector PRIVATE comdlg32)

foreach(TARGET pvz-cheats pvz-cheats-injector)
    target_compile_definitions(${TARGET} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
                                                   UNICODE _UNICODE)

    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /W4 /permissive- /utf-8)
    endif()
endforeach()
