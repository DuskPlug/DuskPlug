cmake_minimum_required(VERSION 3.16)
project(DuskPlug LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DuskPlug WIN32
    src/main.cpp
    src/config.cpp
    src/crypto.cpp
    src/http_win.cpp
    src/tuya_client.cpp
    src/json_util.cpp
    src/solar.cpp
    src/location_win.cpp
    src/activity_win.cpp
    src/smart_mode.cpp
    src/schedule.cpp
    src/settings_dialog.cpp
    src/app.rc
)

target_include_directories(DuskPlug PRIVATE src)

target_link_libraries(DuskPlug PRIVATE
    winhttp
    advapi32
    shell32
    comctl32
    ole32
    user32
    gdi32
    wtsapi32
)

# Place exe in project root (config lives in %APPDATA%\SMART\config.json)
set_target_properties(DuskPlug PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/.."
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/.."
)
