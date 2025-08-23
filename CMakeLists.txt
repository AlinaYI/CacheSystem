cmake_minimum_required(VERSION 3.10)
project(KCacheSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include header file directory
include_directories(include)

# Automatically add all src/*.cpp files (if there are other non-template implementations)
file(GLOB SRC_FILES src/*.cpp)

# Find Google Test
find_package(GTest REQUIRED)

# Create executable (test LRU + LRU-K)
add_executable(test_LruOnly
    test/test_LruOnly.cpp
    ${SRC_FILES}
)

# Create executable (test LFU)
add_executable(test_LfuCache
    test/test_LfuCache.cpp
    ${SRC_FILES}
)

# Create executable (test ARC)
add_executable(test_ArcCache
    test/test_ArcCache.cpp
    ${SRC_FILES}
)

add_executable(test_ArcNew
    test/test_ArcNew.cpp
    ${SRC_FILES}
)

# Link Google Test library
target_link_libraries(test_LruOnly GTest::gtest_main)
target_link_libraries(test_LfuCache GTest::gtest_main)
target_link_libraries(test_ArcCache GTest::gtest_main)
target_link_libraries(test_ArcNew GTest::gtest_main)

# Enable compilation optimization and warnings (recommended)
target_compile_options(test_LruOnly PRIVATE -Wall -Wextra -O2)
target_compile_options(test_LfuCache PRIVATE -Wall -Wextra -O2)
target_compile_options(test_ArcCache PRIVATE -Wall -Wextra -O2)
target_compile_options(test_ArcNew PRIVATE -Wall -Wextra -O2)

# Prompt information
message(STATUS "✅ CMake configured: using template .tpp via header inclusion")
