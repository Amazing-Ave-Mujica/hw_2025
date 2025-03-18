cmake_minimum_required(VERSION 3.8)
project(CodeCraft)             

set(CMAKE_CXX_STANDARD      17)
set(CMAKE_C_STANDARD        11)

set(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/build)
option(BUILD_SHARED_LIBS  ""  OFF)

include_directories(src/include)
aux_source_directory(src SRC)

add_executable(code_craft ${SRC})

add_custom_target(format
        clang-format -i ../src/*.cpp ../src/include/*.h
)

add_custom_target(check-clang-tidy
        clang-tidy -p ${CMAKE_BINARY_DIR} -header-filter=. ../src/*.cpp
)

add_custom_target(run
  python3 ../tools/run.py ../tools/interactor.exe ../data/sample_practice.in ../code_craft.exe
)