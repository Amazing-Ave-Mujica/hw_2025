cmake_minimum_required(VERSION 3.8)
project(CodeCraft)             

set(CMAKE_CXX_STANDARD      17)
set(CMAKE_C_STANDARD        11)

set(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/)
option(BUILD_SHARED_LIBS  ""  OFF)

include_directories(src/include)

aux_source_directory(src/include cur_src_include)
aux_source_directory(src cur_src)


add_executable(code_craft ${cur_src} ${cur_src_include})