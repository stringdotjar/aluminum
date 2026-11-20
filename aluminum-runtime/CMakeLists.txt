cmake_minimum_required(VERSION 3.20)
project(aluminum_runtime LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Writes compile_commands.json, which clangd and clang-tidy need to understand
# the code.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Turns on AddressSanitizer and UndefinedBehaviorSanitizer, which catch memory
# bugs in the runtime and GC.
option(ALUMINUM_SANITIZE "Build with ASan and UBSan" OFF)

add_library(aluminum_runtime STATIC src/runtime.c)
target_include_directories(aluminum_runtime PUBLIC include)
target_compile_options(aluminum_runtime PRIVATE
  -Wall -Wextra -Wpedantic -Werror)

if(ALUMINUM_SANITIZE)
  add_compile_options(
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
  target_compile_options(aluminum_runtime PRIVATE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(runtime_test tests/runtime_test.c)
  target_link_libraries(runtime_test PRIVATE aluminum_runtime)
  target_compile_options(runtime_test PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME runtime_test COMMAND runtime_test)
endif()
