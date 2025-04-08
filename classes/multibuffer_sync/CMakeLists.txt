cmake_minimum_required(VERSION 3.16)
project(SyncBufferTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Opcional para ver todos los warnings
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unknown-pragmas)

# Tu fuente (ajusta el path a tu multibuffer_sync.h si está en otra carpeta)
add_executable(sync_buffer_test unit_test.cpp)

# Si tu multibuffer_sync.h depende de otros headers, añade más aquí
# target_include_directories(sync_buffer_test PRIVATE path_to_dependencies)

