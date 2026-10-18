cmake_minimum_required(VERSION 3.16)
project(pfs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Сборка с проверкой памяти и неопределённого поведения (на Linux/WSL, на MinGW GCC этих библиотек нет):
#   cmake -S . -B build-san -DPFS_SANITIZE=ON
option(PFS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Всё, кроме main.cpp, собирается в библиотеку: так отдельные модули можно подключать к тестам
# (например, аллокатор можно проверять, не собирая всю файловую систему).
add_library(pfs_core STATIC
    Allocator.cpp
    AuthorizedUsers.cpp
    CommonFileDescriptor.cpp
    EncryptedFileDescriptor.cpp
    Encryption.cpp
    FileACList.cpp
    FileAccessEntry.cpp
    FileSystem.cpp
    FileTable.cpp
    FolderACList.cpp
    FolderAccessEntry.cpp
    FolderDescriptor.cpp
    Menu.cpp
    Parser.cpp
    Resolver.cpp
    StreamDescriptor.cpp
    StreamTable.cpp
    Superblock.cpp
    TableNode.cpp
    User.cpp
    UsersTable.cpp
)
target_include_directories(pfs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# -Wno-sign-compare: в коде много сравнений int с size_t, уберём, когда дойдём до чистки
target_compile_options(pfs_core PUBLIC -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter)

add_executable(pfs main.cpp)
target_link_libraries(pfs PRIVATE pfs_core)

if(PFS_SANITIZE)
    target_compile_options(pfs_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(pfs PRIVATE -fsanitize=address,undefined)
endif()

# На Windows (MinGW) вшиваем библиотеки в exe, чтобы он запускался без DLL из MSYS2 в PATH
if(MINGW)
    target_link_options(pfs PRIVATE -static)
endif()

# Сценарные тесты:  ctest --test-dir build
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.py)
    enable_testing()
    add_test(NAME scenarios
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.py --exe $<TARGET_FILE:pfs>)
endif()
