cmake_minimum_required(VERSION 3.16)
project(activity-defaults-switcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# The static libraries are linked into the kded6 shared module, so they must
# be position-independent.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# KDE install locations (${KDE_INSTALL_PLUGINDIR}, ${KDE_INSTALL_KCMDIR}, …)
# used by the install() rules below. Included before find_package(Qt6) so
# KDEInstallDirs — not Qt's GNUInstallDirs-equivalent — owns CMAKE_INSTALL_*.
# 5.240 is the ECM release that defaults to Qt6 when Qt6 isn't found yet.
find_package(ECM 5.240 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# kded6 discovers modules under Qt's plugin directory, which lives in /usr on
# system installs. Default the prefix there (instead of CMake's /usr/local) so
# a plain `make install` lands the module where kded6 finds it; this also makes
# KDEInstallDirs adopt Qt's multi-arch path scheme. Explicit -DCMAKE_INSTALL_PREFIX
# still wins.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH
        "Install path prefix, prepended onto install directories." FORCE)
endif()

include(KDEInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Test)
find_package(KF6Service REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6CoreAddons REQUIRED)

add_library(ads-domain STATIC src/domain.cpp)
target_include_directories(ads-domain PUBLIC src)
target_link_libraries(ads-domain PUBLIC Qt6::Core)

add_library(ads-writer STATIC src/writer.cpp)
target_include_directories(ads-writer PUBLIC src)
target_link_libraries(ads-writer PUBLIC ads-domain KF6::Service KF6::ConfigCore)

add_library(ads-switcher STATIC src/switcher.cpp)
target_include_directories(ads-switcher PUBLIC src)
target_link_libraries(ads-switcher PUBLIC ads-domain ads-writer)

add_library(ads-store STATIC src/store.cpp)
target_include_directories(ads-store PUBLIC src)
target_link_libraries(ads-store PUBLIC ads-domain KF6::ConfigCore)

target_link_libraries(ads-switcher PUBLIC ads-store)

enable_testing()

add_executable(tst_resolver tests/tst_resolver.cpp)
target_link_libraries(tst_resolver PRIVATE ads-domain Qt6::Test)
add_test(NAME resolver COMMAND tst_resolver)

add_executable(tst_writer tests/tst_writer.cpp)
target_link_libraries(tst_writer PRIVATE ads-writer Qt6::Test)
add_test(NAME writer COMMAND tst_writer)

add_executable(tst_switcher tests/tst_switcher.cpp)
target_link_libraries(tst_switcher PRIVATE ads-switcher ads-writer Qt6::Test)
add_test(NAME switcher COMMAND tst_switcher)

add_executable(tst_store tests/tst_store.cpp)
target_link_libraries(tst_store PRIVATE ads-store Qt6::Test)
add_test(NAME store COMMAND tst_store)

# --- kded6 module (ticket 04) ---
# The KDEDModule base class and the KActivities consumer come from the
# KF6DBusAddons and PlasmaActivities dev packages. When those -dev packages
# are not installed system-wide, scripts/setup-deps.sh stages them under
# ./deps (gitignored) and the prefix below makes find_package see them.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/usr")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/usr")
endif()

find_package(KF6DBusAddons QUIET)
find_package(PlasmaActivities QUIET)

if(KF6DBusAddons_FOUND AND PlasmaActivities_FOUND)
    # The module and its JSON are installed side by side into the kded plugin
    # directory; X-KDE-Kded-autoload makes kded6 load it at session start.
    add_library(activity-defaults-switcher-kded MODULE src/module.cpp src/kactivities_backend.cpp src/activity_backend.cpp src/module.json)
    target_include_directories(activity-defaults-switcher-kded PRIVATE src)
    target_link_libraries(activity-defaults-switcher-kded PRIVATE
        ads-switcher
        KF6::DBusAddons
        Plasma::Activities
        KF6::CoreAddons
    )

    install(TARGETS activity-defaults-switcher-kded
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)
    install(FILES src/module.json
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)

    # kded shell tests (ticket 08): instantiate the module with injected fakes so
    # it never constructs KActivities/D-Bus objects or touches a live config.
    add_executable(tst_module
        tests/tst_module.cpp
        src/module.cpp
        src/kactivities_backend.cpp
        src/activity_backend.cpp
    )
    target_include_directories(tst_module PRIVATE src)
    target_link_libraries(tst_module PRIVATE
        ads-switcher
        ads-writer
        ads-store
        KF6::DBusAddons
        Plasma::Activities
        KF6::CoreAddons
        Qt6::Test
    )
    add_test(NAME module COMMAND tst_module)
endif()

# --- uninstall ---
# CMake has no built-in uninstall. This custom target removes every file
# recorded in install_manifest.txt (written by `cmake --install`); see
# cmake_uninstall.cmake.in for the removal script.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    @ONLY)
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
