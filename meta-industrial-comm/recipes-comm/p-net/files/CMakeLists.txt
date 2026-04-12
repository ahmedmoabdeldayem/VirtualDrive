cmake_minimum_required(VERSION 3.16)
project(pnet C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

# ── Version ────────────────────────────────────────────────────────────────
set(PNET_VERSION_MAJOR 1)
set(PNET_VERSION_MINOR 0)
set(PNET_VERSION_PATCH 2)
set(PNET_GIT_REVISION  "ca4f37c")

# ── Feature options ────────────────────────────────────────────────────────
set(PNET_OPTION_FAST_STARTUP         0)
set(PNET_OPTION_PARAMETER_SERVER     0)
set(PNET_OPTION_IR                   1)
set(PNET_OPTION_SR                   1)
set(PNET_OPTION_REDUNDANCY           0)
set(PNET_OPTION_AR_VENDOR_BLOCKS     1)
set(PNET_OPTION_RS                   0)
set(PNET_OPTION_MC_CR                1)
set(PNET_OPTION_SRL                  0)
set(PNET_OPTION_SNMP                 0)
set(PNET_OPTION_DRIVER_ENABLE        0)
set(PNET_USE_ATOMICS                 1)

# ── Memory limits ──────────────────────────────────────────────────────────
set(PNET_MAX_AR                              2)
set(PNET_MAX_API                             1)
set(PNET_MAX_CR                              2)
set(PNET_MAX_SLOTS                           5)
set(PNET_MAX_SUBSLOTS                        3)
set(PNET_MAX_DFP_IOCR                        0)
set(PNET_MAX_PHYSICAL_PORTS                  1)
set(PNET_MAX_LOG_BOOK_ENTRIES               16)
set(PNET_MAX_ALARMS                          3)
set(PNET_MAX_ALARM_PAYLOAD_DATA_SIZE        28)
set(PNET_MAX_DIAG_ITEMS                    100)
set(PNET_MAX_DIAG_MANUF_DATA_SIZE            0)
set(PNET_MAX_MC_CR                           1)
set(PNET_MAX_AR_VENDOR_BLOCKS                1)
set(PNET_MAX_AR_VENDOR_BLOCK_DATA_LENGTH   512)
set(PNET_MAX_MAN_SPECIFIC_FAST_STARTUP_DATA_LENGTH 0)
set(PNET_MAX_SESSION_BUFFER_SIZE          4096)
set(PNET_MAX_DIRECTORYPATH_SIZE            240)
set(PNET_MAX_FILENAME_SIZE                  48)
set(PNET_MAX_PORT_DESCRIPTION_SIZE          64)

# ── Logging (OFF = disabled at compile time) ───────────────────────────────
set(LOG_LEVEL     "WARNING")
set(PF_ETH_LOG    "OFF")
set(PF_LLDP_LOG   "OFF")
set(PF_SNMP_LOG   "OFF")
set(PF_CPM_LOG    "OFF")
set(PF_PPM_LOG    "OFF")
set(PF_DCP_LOG    "OFF")
set(PF_RPC_LOG    "OFF")
set(PF_ALARM_LOG  "OFF")
set(PF_AL_BUF_LOG "OFF")
set(PF_PNAL_LOG   "OFF")
set(PNET_LOG      "OFF")

# ── Generated headers ──────────────────────────────────────────────────────
configure_file(pnet_options.h.in ${CMAKE_CURRENT_BINARY_DIR}/pnet_options.h)
configure_file(pnet_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/pnet_version.h)

# ── Source files ───────────────────────────────────────────────────────────
file(GLOB PNET_COMMON  src/common/*.c)
file(GLOB PNET_DEVICE  src/device/*.c)
# Exclude files for disabled options so they don't access absent struct fields
list(REMOVE_ITEM PNET_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/src/common/pf_snmp.c)
# src/drivers/ contains hardware-specific LAN9662 code — excluded

add_library(pnet STATIC
    ${PNET_COMMON}
    ${PNET_DEVICE}
    osal_linux.c
    pnal_linux.c
)

target_include_directories(pnet
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}       # osal.h, osal_log.h, pnal_sys.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/device
)

target_compile_definitions(pnet PRIVATE _GNU_SOURCE)
target_link_libraries(pnet PRIVATE Threads::Threads rt)

# ── Install ────────────────────────────────────────────────────────────────
install(TARGETS pnet ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/osal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/osal_log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pnal_sys.h
    ${CMAKE_CURRENT_BINARY_DIR}/pnet_options.h
    ${CMAKE_CURRENT_BINARY_DIR}/pnet_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
