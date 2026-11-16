cmake_minimum_required(VERSION 3.20)
project(pos VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(pos_domain STATIC src/domain.cpp)
target_compile_features(pos_domain PUBLIC cxx_std_20)
target_include_directories(pos_domain PUBLIC include)

add_library(pos_application STATIC src/application.cpp)
target_link_libraries(pos_application PUBLIC pos_domain)

add_library(pos_adapters STATIC src/adapters.cpp src/console.cpp)
target_link_libraries(pos_adapters PUBLIC pos_application)

add_executable(pos src/main.cpp)
target_link_libraries(pos PRIVATE pos_adapters)

include(CTest)
if(BUILD_TESTING)
    add_executable(pos_domain_tests tests/domain_tests.cpp)
    target_link_libraries(pos_domain_tests PRIVATE pos_domain)
    add_test(NAME domain COMMAND pos_domain_tests)

    add_executable(pos_application_tests tests/application_tests.cpp)
    target_link_libraries(pos_application_tests PRIVATE pos_adapters)
    add_test(NAME application COMMAND pos_application_tests)
endif()
