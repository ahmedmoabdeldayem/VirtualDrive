/* osal_log.h — logging macros for p-net (matches rtlabs-com/osal interface) */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_LEVEL_DEBUG    0x00
#define LOG_LEVEL_INFO     0x01
#define LOG_LEVEL_WARNING  0x02
#define LOG_LEVEL_ERROR    0x03
#define LOG_LEVEL_FATAL    0x04
#define LOG_LEVEL_MASK     0x0F

#define LOG_STATE_ON       0x80
#define LOG_STATE_OFF      0x00

typedef void (*os_log_t) (uint8_t type, const char * fmt, ...);
extern os_log_t os_log;

#define LOG_ENABLED(type) \
   (((type) & LOG_LEVEL_MASK) >= (LOG_LEVEL & LOG_LEVEL_MASK) && \
    ((type) & LOG_STATE_ON))

#define LOG(type, ...) \
   do { if (LOG_ENABLED (type)) os_log ((type), __VA_ARGS__); } while (0)

#define LOG_DEBUG(type, ...)   LOG ((LOG_LEVEL_DEBUG   | (type)), __VA_ARGS__)
#define LOG_INFO(type, ...)    LOG ((LOG_LEVEL_INFO    | (type)), __VA_ARGS__)
#define LOG_WARNING(type, ...) LOG ((LOG_LEVEL_WARNING | (type)), __VA_ARGS__)
#define LOG_ERROR(type, ...)   LOG ((LOG_LEVEL_ERROR   | (type)), __VA_ARGS__)
#define LOG_FATAL(type, ...)   LOG ((LOG_LEVEL_FATAL   | (type)), __VA_ARGS__)

#define LOG_DEBUG_ENABLED(type)   LOG_ENABLED (LOG_LEVEL_DEBUG   | (type))
#define LOG_INFO_ENABLED(type)    LOG_ENABLED (LOG_LEVEL_INFO    | (type))
#define LOG_WARNING_ENABLED(type) LOG_ENABLED (LOG_LEVEL_WARNING | (type))
#define LOG_ERROR_ENABLED(type)   LOG_ENABLED (LOG_LEVEL_ERROR   | (type))
#define LOG_FATAL_ENABLED(type)   LOG_ENABLED (LOG_LEVEL_FATAL   | (type))

#ifdef __cplusplus
}
#endif
