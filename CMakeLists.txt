cmake_minimum_required(VERSION 3.16)
project(activity-defaults-switcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# The static libraries are linked into the kded6 shared module, so they must
# be position-independent.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
    # Installed to ${KDE_INSTALL_PLUGINDIR}/kf6/kded by packaging (out of scope
    # here); the JSON's X-KDE-Kded-autoload makes kded6 load it at session start.
    add_library(activity-defaults-switcher-kded MODULE src/module.cpp src/module.json)
    target_include_directories(activity-defaults-switcher-kded PRIVATE src)
    target_link_libraries(activity-defaults-switcher-kded PRIVATE
        ads-switcher
        KF6::DBusAddons
        Plasma::Activities
        KF6::CoreAddons
    )
endif()
