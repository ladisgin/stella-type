cmake_minimum_required(VERSION 3.20)
project(stella_typechecker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(BNFC_EXECUTABLE bnfc REQUIRED)
find_package(BISON 3.0 REQUIRED)
find_package(FLEX REQUIRED)

set(GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${GENERATED}")
execute_process(COMMAND "${BNFC_EXECUTABLE}" --cpp -o "${GENERATED}" "${CMAKE_CURRENT_SOURCE_DIR}/Stella.cf" RESULT_VARIABLE BNFC_RESULT)
                
if(NOT BNFC_RESULT EQUAL 0)
  message(FATAL_ERROR "BNFC parser generation failed")
endif()

file(READ "${GENERATED}/Stella.y" PARSER_SOURCE)
string(REPLACE "%defines \"Bison.H\"" "" PARSER_SOURCE "${PARSER_SOURCE}")
file(WRITE "${GENERATED}/Stella.y" "${PARSER_SOURCE}")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS Stella.cf)

BISON_TARGET(StellaParser "${GENERATED}/Stella.y" "${GENERATED}/Parser.C"
             DEFINES_FILE "${GENERATED}/Bison.H" COMPILE_FLAGS "-pstella_")
FLEX_TARGET(StellaLexer "${GENERATED}/Stella.l" "${GENERATED}/Lexer.C" COMPILE_FLAGS "-Pstella_")
ADD_FLEX_BISON_DEPENDENCY(StellaLexer StellaParser)

add_executable(typechecker src/main.cpp lib/checker.cpp "${GENERATED}/Absyn.C" "${GENERATED}/Buffer.C"
               "${GENERATED}/Printer.C" ${BISON_StellaParser_OUTPUTS} ${FLEX_StellaLexer_OUTPUTS})

target_include_directories(typechecker PRIVATE "${GENERATED}" "${CMAKE_CURRENT_SOURCE_DIR}/lib")
