cmake_minimum_required(VERSION 3.24)
project(gura VERSION 0.1.0 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GuraCompilerOptions)
include(GuraLLVM)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Catch2 REQUIRED)

add_library(gura_basic
  src/basic/SourceManager.cpp
  src/basic/Diagnostic.cpp
)
target_include_directories(gura_basic PUBLIC include)
target_link_libraries(gura_basic PUBLIC fmt::fmt)
gura_apply_common_options(gura_basic)

add_library(gura_lexer
  src/lexer/Lexer.cpp
)
target_include_directories(gura_lexer PUBLIC include)
target_link_libraries(gura_lexer PUBLIC gura_basic)
gura_apply_common_options(gura_lexer)

add_library(gura_ast INTERFACE)
target_include_directories(gura_ast INTERFACE include)

add_library(gura_parser
  src/parser/Parser.cpp
)
target_include_directories(gura_parser PUBLIC include)
target_link_libraries(gura_parser PUBLIC gura_basic gura_lexer gura_ast)
gura_apply_common_options(gura_parser)

add_library(gura_hir
  src/hir/Sema.cpp
)
target_include_directories(gura_hir PUBLIC include)
target_link_libraries(gura_hir PUBLIC gura_ast gura_basic)
gura_apply_common_options(gura_hir)

add_library(gura_region
  src/region/RegionIR.cpp
  src/region/RegionVerifier.cpp
)
target_include_directories(gura_region PUBLIC include)
target_link_libraries(gura_region PUBLIC gura_basic)
gura_apply_common_options(gura_region)

add_library(gura_codegen_llvm
  src/codegen/LLVMCodeGen.cpp
)
target_include_directories(gura_codegen_llvm PUBLIC include)
target_link_libraries(gura_codegen_llvm PUBLIC gura_basic)
gura_link_llvm(gura_codegen_llvm)
gura_apply_common_options(gura_codegen_llvm)

add_library(gura_runtime
  runtime/src/Region.cpp
  runtime/src/Cown.cpp
  runtime/src/Freeze.cpp
  runtime/src/Merge.cpp
)
target_include_directories(gura_runtime PUBLIC runtime/include)
target_link_libraries(gura_runtime PUBLIC spdlog::spdlog)
gura_apply_common_options(gura_runtime)

add_library(gura_driver
  src/driver/Driver.cpp
)
target_include_directories(gura_driver PUBLIC include)
target_link_libraries(gura_driver PUBLIC gura_basic gura_lexer gura_parser gura_hir gura_region gura_codegen_llvm spdlog::spdlog)
gura_apply_common_options(gura_driver)

add_executable(gura tools/gura/main.cpp)
target_link_libraries(gura PRIVATE gura_driver)
gura_apply_common_options(gura)

include(CTest)
if(BUILD_TESTING)
  add_executable(gura_tests
    tests/lexer/basic_tokens_test.cpp
    tests/parser/parse_minimal_fn_test.cpp
    tests/runtime/region_runtime_test.cpp
    tests/codegen/codegen_basic_test.cpp
    tests/codegen/codegen_array_test.cpp
    tests/codegen/codegen_loop_test.cpp
    tests/codegen/codegen_struct_test.cpp
    tests/codegen/codegen_field_assign_test.cpp
    tests/codegen/codegen_method_test.cpp
    tests/codegen/codegen_associated_function_test.cpp
    tests/parser/parse_core_exprs_test.cpp
    tests/parser/parse_call_binary_test.cpp
    tests/parser/parse_array_test.cpp
    tests/parser/parse_if_expr_test.cpp
    tests/parser/parse_assignment_test.cpp
    tests/parser/parse_while_expr_test.cpp
    tests/parser/parse_struct_field_test.cpp
    tests/parser/parse_new_init_test.cpp
    tests/parser/parse_method_test.cpp
    tests/parser/parse_impl_method_test.cpp
    tests/parser/parse_trait_test.cpp
    tests/sema/sema_core_language_test.cpp
    tests/sema/sema_call_binary_test.cpp
    tests/sema/sema_array_test.cpp
    tests/sema/sema_if_expr_test.cpp
    tests/sema/sema_assignment_test.cpp
    tests/sema/sema_while_expr_test.cpp
    tests/sema/sema_struct_field_test.cpp
    tests/sema/sema_region_field_test.cpp
    tests/sema/sema_new_init_test.cpp
    tests/sema/sema_method_test.cpp
    tests/sema/sema_impl_method_test.cpp
    tests/sema/sema_associated_function_test.cpp
    tests/sema/sema_trait_test.cpp
  )
  target_link_libraries(gura_tests PRIVATE gura_lexer gura_parser gura_hir gura_codegen_llvm gura_runtime Catch2::Catch2WithMain)
  gura_apply_common_options(gura_tests)
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(gura_tests)

  find_program(GURA_CLANGXX clang++)
  if(GURA_CLANGXX)
    add_test(
      NAME gura_build_return0
      COMMAND $<TARGET_FILE:gura> build ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/return0.gura -o ${CMAKE_CURRENT_BINARY_DIR}/return0
    )
    set_tests_properties(gura_build_return0 PROPERTIES ENVIRONMENT "GURA_CLANG=${GURA_CLANGXX}")
    add_test(NAME gura_run_return0 COMMAND ${CMAKE_CURRENT_BINARY_DIR}/return0)
    set_tests_properties(gura_run_return0 PROPERTIES DEPENDS gura_build_return0)

    add_test(
      NAME gura_build_hello
      COMMAND $<TARGET_FILE:gura> build ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/hello.gura -o ${CMAKE_CURRENT_BINARY_DIR}/hello
    )
    set_tests_properties(gura_build_hello PROPERTIES ENVIRONMENT "GURA_CLANG=${GURA_CLANGXX}")
    add_test(NAME gura_run_hello COMMAND ${CMAKE_CURRENT_BINARY_DIR}/hello)
    set_tests_properties(gura_run_hello PROPERTIES DEPENDS gura_build_hello PASS_REGULAR_EXPRESSION "hello")

    add_test(
      NAME gura_build_array_swap
      COMMAND $<TARGET_FILE:gura> build ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/array_swap.gura -o ${CMAKE_CURRENT_BINARY_DIR}/array_swap
    )
    set_tests_properties(gura_build_array_swap PROPERTIES ENVIRONMENT "GURA_CLANG=${GURA_CLANGXX}")
    add_test(NAME gura_run_array_swap COMMAND ${CMAKE_CURRENT_BINARY_DIR}/array_swap)
    set_tests_properties(gura_run_array_swap PROPERTIES DEPENDS gura_build_array_swap)

    add_test(
      NAME gura_build_array_param_swap
      COMMAND $<TARGET_FILE:gura> build ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/array_param_swap.gura -o ${CMAKE_CURRENT_BINARY_DIR}/array_param_swap
    )
    set_tests_properties(gura_build_array_param_swap PROPERTIES ENVIRONMENT "GURA_CLANG=${GURA_CLANGXX}")
    add_test(NAME gura_run_array_param_swap COMMAND ${CMAKE_CURRENT_BINARY_DIR}/array_param_swap)
    set_tests_properties(gura_run_array_param_swap PROPERTIES DEPENDS gura_build_array_param_swap)
  endif()
endif()
