cmake_minimum_required(VERSION 3.16)
project(drive-simulator LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compile flags
add_compile_options(-Wall -Wextra -Werror -O2)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -O1)
    add_link_options(-fsanitize=address)
endif()

find_package(Threads REQUIRED)

# --- soes ---
find_library(SOES_LIB NAMES soes PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib
             NO_DEFAULT_PATH REQUIRED)
find_library(OSAL_LIB NAMES osal PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib
             NO_DEFAULT_PATH QUIET)
find_path(SOES_INCLUDE_DIR NAMES soes/esc.h
          PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include NO_DEFAULT_PATH REQUIRED)

# --- p-net ---
find_library(PNET_LIB NAMES pnet PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib
             NO_DEFAULT_PATH REQUIRED)
find_path(PNET_INCLUDE_DIR NAMES pnet_api.h
          PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include NO_DEFAULT_PATH REQUIRED)

# --- OpENer ---
find_library(OPENER_LIB NAMES EIPStack opener
             PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH REQUIRED)
find_path(OPENER_INCLUDE_DIR NAMES opener_api.h
          PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include/opener NO_DEFAULT_PATH REQUIRED)

set(SOURCES
    src/main.cpp
    src/DriveSimulator.cpp
    src/drive/DriveStateMachine.cpp
    src/drive/MotorModel.cpp
    src/comm/EtherCATAdapter.cpp
    src/comm/PROFINETAdapter.cpp
    src/comm/EtherNetIPAdapter.cpp
    src/rt/CyclicThread.cpp
    src/hal/SerialLoopback.cpp
    src/monitoring/DriveMonitor.cpp
)

add_executable(drive-simulator ${SOURCES})

target_include_directories(drive-simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SOES_INCLUDE_DIR}
    ${PNET_INCLUDE_DIR}
    ${OPENER_INCLUDE_DIR}
)

set(PROTOCOL_LIBS ${SOES_LIB} ${PNET_LIB} ${OPENER_LIB})
if(OSAL_LIB)
    list(APPEND PROTOCOL_LIBS ${OSAL_LIB})
endif()

target_link_libraries(drive-simulator PRIVATE
    Threads::Threads
    rt
    ${PROTOCOL_LIBS}
)

install(TARGETS drive-simulator RUNTIME DESTINATION bin)

# Unit tests — excluded from Yocto build (BUILD_TESTING=OFF in recipe)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
