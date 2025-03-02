cmake_minimum_required(VERSION 3.15)
project(engine C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CLANGD_FILE ${CMAKE_BINARY_DIR}/.clangd)
execute_process(COMMAND ${CMAKE_C_COMPILER} --cflags OUTPUT_VARIABLE CLANGD_FLAGS_TO_ADD)
separate_arguments(CLANGD_FLAGS_TO_ADD UNIX_COMMAND "${CLANGD_FLAGS_TO_ADD}")
list(JOIN CLANGD_FLAGS_TO_ADD ", " CLANGD_FLAGS_TO_ADD)
set(CLANGD_TEMPLATE ${PROJECT_SOURCE_DIR}/.clangd.in)
configure_file(${CLANGD_TEMPLATE} ${CLANGD_FILE})

file(GLOB_RECURSE ENGINE_SRCS ${PROJECT_SOURCE_DIR}/engine/*.c)
add_executable(engine ${ENGINE_SRCS})
set_target_properties(engine PROPERTIES C_STANDARD 99 LINK_FLAGS "-sEXPORTED_RUNTIME_METHODS=ccall -sASYNCIFY -sWASM=1 -sUSE_WEBGPU=1 --preload-file ../assets/")
target_include_directories(engine PRIVATE ${PROJECT_SOURCE_DIR}/include)
