cmake_minimum_required(VERSION 3.16)
project(activity-defaults-switcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)
find_package(KF6Service REQUIRED)
find_package(KF6Config REQUIRED)

add_library(ads-domain STATIC src/domain.cpp)
target_include_directories(ads-domain PUBLIC src)
target_link_libraries(ads-domain PUBLIC Qt6::Core)

add_library(ads-writer STATIC src/writer.cpp)
target_include_directories(ads-writer PUBLIC src)
target_link_libraries(ads-writer PUBLIC ads-domain KF6::Service KF6::ConfigCore)

enable_testing()

add_executable(tst_resolver tests/tst_resolver.cpp)
target_link_libraries(tst_resolver PRIVATE ads-domain Qt6::Test)
add_test(NAME resolver COMMAND tst_resolver)

add_executable(tst_writer tests/tst_writer.cpp)
target_link_libraries(tst_writer PRIVATE ads-writer Qt6::Test)
add_test(NAME writer COMMAND tst_writer)
