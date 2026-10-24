cmake_minimum_required(VERSION 3.16)
project(drive-simulator LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror -O2)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -O1)
    add_link_options(-fsanitize=address)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Protocol libraries — all OPTIONAL.
# The adapters compile to stubs when the library is absent so the binary
# always builds. Full integration is enabled when the library is found.
# ---------------------------------------------------------------------------

find_library(SOES_LIB  NAMES soes  PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_library(OSAL_LIB  NAMES osal  PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_path(SOES_INC     NAMES soes/esc.h esc.h
          PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include
                ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include/soes
          NO_DEFAULT_PATH QUIET)

find_library(PNET_LIB  NAMES pnet  PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_path(PNET_INC     NAMES pnet_api.h
          PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include NO_DEFAULT_PATH QUIET)

# OpENer builds multiple libraries — find the main CIP stack library
find_library(OPENER_LIB NAMES CIP
             PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_library(OPENER_ENET_LIB  NAMES ENET_ENCAP   PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_library(OPENER_POSIX_LIB NAMES POSIXPLATFORM PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_library(OPENER_GEN_LIB   NAMES PLATFORM_GENERIC PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_library(OPENER_UTILS_LIB NAMES Utils         PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_library(OPENER_NVDATA_LIB NAMES NVDATA        PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/lib NO_DEFAULT_PATH QUIET)
find_path(OPENER_INC   NAMES opener_api.h
          PATHS ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include/opener
                ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include
          NO_DEFAULT_PATH QUIET)

message(STATUS "SOES   : ${SOES_LIB}  headers: ${SOES_INC}")
message(STATUS "p-net  : ${PNET_LIB}  headers: ${PNET_INC}")
message(STATUS "OpENer : ${OPENER_LIB} headers: ${OPENER_INC}")

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
)

# Conditionally add library includes and link flags
if(SOES_LIB AND SOES_INC)
    target_compile_definitions(drive-simulator PRIVATE HAVE_SOES=1)
    target_include_directories(drive-simulator PRIVATE ${SOES_INC})
    target_link_libraries(drive-simulator PRIVATE ${SOES_LIB})
    if(OSAL_LIB)
        target_link_libraries(drive-simulator PRIVATE ${OSAL_LIB})
    endif()
    message(STATUS "EtherCAT : full SOES integration")
else()
    message(STATUS "EtherCAT : stub mode (SOES not found)")
endif()

if(PNET_LIB AND PNET_INC)
    target_compile_definitions(drive-simulator PRIVATE HAVE_PNET=1)
    target_include_directories(drive-simulator PRIVATE ${PNET_INC})
    target_link_libraries(drive-simulator PRIVATE ${PNET_LIB})
    message(STATUS "PROFINET : full p-net integration")
else()
    message(STATUS "PROFINET : stub mode (p-net not found)")
endif()

if(OPENER_LIB AND OPENER_INC)
    target_compile_definitions(drive-simulator PRIVATE HAVE_OPENER=1)
    target_include_directories(drive-simulator PRIVATE ${OPENER_INC})
    target_link_libraries(drive-simulator PRIVATE
        ${OPENER_LIB}
        ${OPENER_ENET_LIB}
        ${OPENER_POSIX_LIB}
        ${OPENER_GEN_LIB}
        ${OPENER_UTILS_LIB}
        ${OPENER_NVDATA_LIB}
    )
    message(STATUS "EtherNet/IP : full OpENer integration")
else()
    message(STATUS "EtherNet/IP : stub mode (OpENer not found)")
endif()

target_link_libraries(drive-simulator PRIVATE Threads::Threads rt)

install(TARGETS drive-simulator RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
