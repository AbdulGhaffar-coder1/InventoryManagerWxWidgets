cmake_minimum_required(VERSION 3.16)
project(InventoryManagerWxWidgets VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(InventoryManager src/main.cpp)
target_link_libraries(InventoryManager PRIVATE ${wxWidgets_LIBRARIES})

if(MSVC)
    target_compile_options(InventoryManager PRIVATE /W4)
else()
    target_compile_options(InventoryManager PRIVATE -Wall -Wextra -Wpedantic)
endif()
