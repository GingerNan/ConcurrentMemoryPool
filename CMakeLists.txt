cmake_minimum_required(VERSION 3.29.0)
project(MemoryPool)

# 启用详细的Makefile输出，方便调试
set(CMAKE_VERBOS_MAKEFILE on)
set(CMAKE_CXX_CFLAGS "$ENV{CXXFLAGS} -rdynamic -03 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

# 添加包含目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 定义库的源文件列表
set(LIB_SRC
    src/CentralCache.cpp
    src/PageCache.cpp
    src/ThreadCache.cpp
)

# 添加一个共享库目标
add_library(${PROJECT_NAME}
SHARED
${LIB_SRC}
)
#add_library(tartarus_static STATIC ${LIB_SRC})
#SET_TARGET_PROPERTIES (tartarus_static PROPERTIES OUTPUT_NAME "tartarus")

# 添加一个可执行文件目标，名称为test，源文件为tests/test.cc
add_executable(test tests/UnitTest.cpp)
# 指定test目标依赖于tartarus库目标
add_dependencies(test MemoryPool)
# 将tartarus库链接到test可执行文件。
target_link_libraries(test MemoryPool)

# 设置可执行文件的输出目录为 `${PROJECT_SOURCE_DIR}/bin`
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 设置库文件的输出目录为 `${PROJECT_SOURCE_DIR}/lib`
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)