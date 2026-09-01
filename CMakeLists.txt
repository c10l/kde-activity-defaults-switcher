cmake_minimum_required(VERSION 3.16)
project(activity-defaults-switcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_library(ads-domain STATIC src/domain.cpp)
target_include_directories(ads-domain PUBLIC src)
target_link_libraries(ads-domain PUBLIC Qt6::Core)

enable_testing()

add_executable(tst_resolver tests/tst_resolver.cpp)
target_link_libraries(tst_resolver PRIVATE ads-domain Qt6::Test)
add_test(NAME resolver COMMAND tst_resolver)
