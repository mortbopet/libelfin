cmake_minimum_required(VERSION 3.1)
project(libelfin CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PythonInterp 3 REQUIRED)

file(GLOB_RECURSE elf_sources ${CMAKE_CURRENT_SOURCE_DIR}/elf/*.cc)
set(elf_headers
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/common.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/data.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf++.hh)
file(GLOB_RECURSE dwarf_sources ${CMAKE_CURRENT_SOURCE_DIR}/dwarf/*.cc)
set(dwarf_headers
    ${CMAKE_CURRENT_SOURCE_DIR}/dwarf/data.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/dwarf/dwarf++.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/dwarf/small_vector.hh)

# Generate to_string implementations for elf and dwarf libraries
set(ENUM_PRINT ${CMAKE_CURRENT_SOURCE_DIR}/elf/enum-print.py)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/elf_to_string.cc
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/genUtils/gen_elf_to_string.cc elf_to_string.cc
    COMMAND ${PYTHON_EXECUTABLE} ${ENUM_PRINT} -u --hex --no-type --mask shf --mask pf -x loos -x hios -x loproc -x hiproc < ${CMAKE_CURRENT_SOURCE_DIR}/elf/data.hh >> elf_to_string.cc
    COMMAND ${CMAKE_COMMAND} -E echo "ELFPP_END_NAMESPACE" >> elf_to_string.cc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dwarf_to_string.cc
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/genUtils/gen_dwarf_to_string.cc dwarf_to_string.cc
    COMMAND ${PYTHON_EXECUTABLE} ${ENUM_PRINT} <  ${CMAKE_CURRENT_SOURCE_DIR}/dwarf/dwarf++.hh >> dwarf_to_string.cc
    COMMAND ${PYTHON_EXECUTABLE} ${ENUM_PRINT} -s _ -u --hex -x hi_user -x lo_user < ${CMAKE_CURRENT_SOURCE_DIR}/dwarf/data.hh >> dwarf_to_string.cc
    COMMAND ${CMAKE_COMMAND} -E echo "DWARFPP_END_NAMESPACE" >> dwarf_to_string.cc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_library(elf++ STATIC
    ${elf_sources}
    ${CMAKE_CURRENT_BINARY_DIR}/elf_to_string.cc
)
set_target_properties(elf++ PROPERTIES
    PUBLIC_HEADER "${elf_headers}")

add_library(dwarf++ STATIC
    ${dwarf_sources}
    ${CMAKE_CURRENT_BINARY_DIR}/dwarf_to_string.cc
)
set_target_properties(dwarf++ PROPERTIES
    PUBLIC_HEADER "${dwarf_headers}")

target_include_directories(elf++ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(dwarf++ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
