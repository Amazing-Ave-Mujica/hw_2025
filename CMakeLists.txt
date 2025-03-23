cmake_minimum_required(VERSION 3.10)
project(CodeCraft)             

set(CMAKE_CXX_STANDARD      17)
set(CMAKE_C_STANDARD        11)

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
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
add_custom_target(run
  python ../tools/run.py ../tools/interactor.exe ../data/sample_practice.in ./code_craft.exe
)
else()
add_custom_target(run
  python ../tools/run.py ../tools/interactor ../data/sample_practice.in ./code_craft
)
endif()

add_custom_target(debug
  python ../tools/run.py ../tools/interactor.exe ../data/sample.in ./code_craft.exe -d
)

add_custom_target(submit
    COMMAND ${CMAKE_COMMAND} -E tar cfv submission.zip --format=zip
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/CMakeLists.txt
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating ZIP archive of /src and CMakeLists.txt"
)