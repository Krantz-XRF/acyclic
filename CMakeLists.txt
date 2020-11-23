cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0091 NEW)

project(acyclic CXX)

# LLVM/Clang dependency
find_package(LLVM REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}")
find_package(Clang REQUIRED)
message(STATUS "Using ClangConfig.cmake in ${Clang_DIR}")

# Use spdlog for logging information
find_package(spdlog CONFIG REQUIRED)

# Use this fmt library in place of C++20 std::format
find_package(fmt CONFIG REQUIRED)

# Use Microsoft.GSL
set(GSL_CXX_STANDARD 17)
find_package(Microsoft.GSL CONFIG REQUIRED)

add_executable(acyclic)
target_sources(acyclic PRIVATE acyclic.cpp)
target_compile_features(acyclic PRIVATE cxx_std_17)
target_include_directories(acyclic SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_directories(acyclic PRIVATE ${LLVM_LIBRARY_DIRS})
target_link_libraries(acyclic PRIVATE
  clangTooling clangBasic clangASTMatchers
  fmt::fmt Microsoft.GSL::GSL spdlog::spdlog)
set_property(TARGET acyclic PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
