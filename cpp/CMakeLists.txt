cmake_minimum_required(VERSION 3.16)
project(DuskPlug LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(READ "${CMAKE_SOURCE_DIR}/../VERSION" DUSKPLUG_VERSION_RAW)
string(STRIP "${DUSKPLUG_VERSION_RAW}" DUSKPLUG_VERSION)
if(DUSKPLUG_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    set(DUSKPLUG_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(DUSKPLUG_VERSION_MINOR ${CMAKE_MATCH_2})
    set(DUSKPLUG_VERSION_PATCH ${CMAKE_MATCH_3})
else()
    message(FATAL_ERROR "VERSION must be semver major.minor.patch")
endif()
configure_file("${CMAKE_SOURCE_DIR}/src/version.h.in" "${CMAKE_SOURCE_DIR}/src/version.h" @ONLY)

set(DUSKPLUG_CORE_SOURCES
    src/config.cpp
    src/crypto.cpp
    src/platform_util.cpp
    src/tuya_client.cpp
    src/json_util.cpp
    src/solar.cpp
    src/schedule.cpp
    src/coords.cpp
    src/smart_mode.cpp
    src/semver.cpp
    src/install_kind.cpp
    src/update_checker.cpp
)

add_library(duskplug_core STATIC ${DUSKPLUG_CORE_SOURCES})
target_include_directories(duskplug_core PUBLIC src)

if(WIN32)
    target_sources(duskplug_core PRIVATE src/http_win.cpp)
    if(MINGW)
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --include-dir=${CMAKE_SOURCE_DIR}/src")
    endif()
    add_executable(DuskPlug WIN32
        src/main.cpp
        src/settings_dialog.cpp
        src/location_win.cpp
        src/location_cli.cpp
        src/location_geolocator.cpp
        src/activity_win.cpp
        src/location_service_win.cpp
        src/brightness_win.cpp
        src/app.rc
    )
    target_sources(duskplug_core PRIVATE src/update_apply_win.cpp)
    target_link_libraries(DuskPlug PRIVATE duskplug_core winhttp advapi32 shell32 comctl32 ole32 oleaut32 uuid user32 gdi32 wtsapi32 wbemuuid PowrProf dxva2)
    set_target_properties(DuskPlug PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    )
elseif(APPLE)
    enable_language(OBJCXX)
    target_sources(duskplug_core PRIVATE src/http_curl.cpp)
    find_package(CURL REQUIRED)
    target_link_libraries(duskplug_core PUBLIC CURL::libcurl)

    add_executable(DuskPlug MACOSX_BUNDLE
        src/macos/main_macos.mm
        src/macos/settings_macos.mm
        src/macos/location_macos.mm
        src/macos/activity_macos.mm
        src/macos/brightness_macos.mm
        src/settings_page.cpp
    )
    set_target_properties(DuskPlug PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    )
    target_sources(duskplug_core PRIVATE src/update_apply_macos.mm)
    target_link_libraries(DuskPlug PRIVATE duskplug_core "-framework AppKit" "-framework CoreLocation" "-framework ServiceManagement" "-framework IOKit" "-framework WebKit")
else()
    target_sources(duskplug_core PRIVATE src/http_curl.cpp)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED libcurl)
    target_include_directories(duskplug_core PUBLIC ${CURL_INCLUDE_DIRS})
    target_link_libraries(duskplug_core PUBLIC ${CURL_LIBRARIES})

    target_sources(duskplug_core PRIVATE src/update_apply_linux.cpp)

    option(DUSKPLUG_BUILD_LINUX_APP "Build Linux tray app (requires GTK/WebKit)" ON)
    if(DUSKPLUG_BUILD_LINUX_APP)
        pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
        pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.0)
        pkg_check_modules(APPINDICATOR REQUIRED ayatana-appindicator3-0.1)
        pkg_check_modules(GEOCLUE REQUIRED IMPORTED_TARGET libgeoclue-2.0)
        pkg_check_modules(SYSTEMD REQUIRED libsystemd)

        add_executable(duskplug
            src/linux/main_linux.cpp
            src/linux/settings_gtk.cpp
            src/linux/location_linux.cpp
            src/linux/activity_linux.cpp
            src/linux/brightness_linux.cpp
            src/settings_page.cpp
        )
        target_include_directories(duskplug PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS} ${APPINDICATOR_INCLUDE_DIRS})
        target_link_libraries(duskplug PRIVATE
            duskplug_core
            ${GTK3_LIBRARIES}
            ${WEBKIT_LIBRARIES}
            ${APPINDICATOR_LIBRARIES}
            PkgConfig::GEOCLUE
            ${SYSTEMD_LIBRARIES})
    endif()
endif()

enable_testing()
# Keep this list in sync with cpp/test.ps1 (Windows test runner).
add_executable(duskplug_tests
    tests/test_main.cpp
    tests/schedule_test.cpp
    tests/json_test.cpp
    tests/crypto_test.cpp
    tests/solar_test.cpp
    tests/config_test.cpp
    tests/coords_test.cpp
    tests/semver_test.cpp
    tests/update_checker_test.cpp
    tests/settings_page_test.cpp
    tests/tray_menu_test.cpp
    tests/brightness_display_test.cpp
    src/settings_page.cpp
    src/tray_menu.cpp
)
target_include_directories(duskplug_tests PRIVATE src tests)
target_link_libraries(duskplug_tests PRIVATE duskplug_core)
if(WIN32)
    target_link_libraries(duskplug_tests PRIVATE winhttp advapi32 shell32 crypt32)
endif()
add_test(NAME duskplug_tests COMMAND duskplug_tests)
