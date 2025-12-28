cmake_minimum_required(VERSION 3.10)
project(TokenizerLab)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tokenizer
    main.cpp
    tokenizer.cpp
    file_processor.cpp
)

# Настройки для Windows
if(WIN32)
    add_definitions(-D_UNICODE -DUNICODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexec-charset=UTF-8")
endif()

# Настройки для Linux/macOS
if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")
endif()