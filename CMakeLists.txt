cmake_minimum_required(VERSION 3.10)
project(KCacheSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含头文件目录
include_directories(include)

# 自动添加所有 src/*.cpp 文件（如果还有其他非模板实现）
file(GLOB SRC_FILES src/*.cpp)

# 查找 Google Test
find_package(GTest REQUIRED)

# 创建可执行文件（测试 LRU + LRU-K）
add_executable(test_LruOnly
    test/test_LruOnly.cpp
    ${SRC_FILES}
)

# 创建可执行文件（测试 LFU）
add_executable(test_LfuCache
    test/test_LfuCache.cpp
    ${SRC_FILES}
)

# # 创建可执行文件（测试 ARC）
# add_executable(test_ArcCache
#     test/test_ArcCache.cpp
#     ${SRC_FILES}
# )

# 链接 Google Test 库
target_link_libraries(test_LruOnly GTest::gtest_main)
target_link_libraries(test_LfuCache GTest::gtest_main)
# target_link_libraries(test_ArcCache GTest::gtest_main)

# 开启编译优化和警告（建议）
target_compile_options(test_LruOnly PRIVATE -Wall -Wextra -O2)
target_compile_options(test_LfuCache PRIVATE -Wall -Wextra -O2)
# target_compile_options(test_ArcCache PRIVATE -Wall -Wextra -O2)

# 提示信息
message(STATUS "✅ CMake configured: using template .tpp via header inclusion")
