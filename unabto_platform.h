#ifndef _UNABTO_PLATFORM_H_
#define _UNABTO_PLATFORM_H_

#include <platforms/unabto_common_types.h>
#include "unabto_platform_types.h"

/**
* Generic typedefs
*/

typedef int ssize_t;

/**
* Socket related definitions
*/

#define NABTO_INVALID_SOCKET -1

/**
* Time related definitions
*/
#define nabtoGetStamp(void) xTaskGetTickCount()
#define nabtoMsec2Stamp

/**
* Logging related definitions
*/

#include "common.h"
#include "uart_if.h"

#define NABTO_LOG_BASIC_PRINT(severity, msg) \
    do {                                     \
        UART_PRINT msg;                      \
        UART_PRINT("\r\n");                  \
    } while (0)

/**
* Other defines
*/

#define NABTO_FATAL_EXIT LOOP_FOREVER();

#endif
