#ifndef RADIOLIB_TYPES_H
#define RADIOLIB_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status values intentionally follow RadioLib's negative-error convention. */
#define RADIOLIB_ERR_NONE                         0
#define RADIOLIB_ERR_UNKNOWN                    -1
#define RADIOLIB_ERR_CHIP_NOT_FOUND             -2
#define RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED   -3
#define RADIOLIB_ERR_PACKET_TOO_LONG            -4
#define RADIOLIB_ERR_TX_TIMEOUT                 -5
#define RADIOLIB_ERR_RX_TIMEOUT                 -6
#define RADIOLIB_ERR_CRC_MISMATCH               -7
#define RADIOLIB_ERR_INVALID_BANDWIDTH          -8
#define RADIOLIB_ERR_INVALID_SPREADING_FACTOR   -9
#define RADIOLIB_ERR_INVALID_CODING_RATE       -10
#define RADIOLIB_ERR_INVALID_FREQUENCY         -11
#define RADIOLIB_ERR_INVALID_OUTPUT_POWER      -12
#define RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH   -13
#define RADIOLIB_ERR_INVALID_GAIN              -14
#define RADIOLIB_ERR_SPI_WRITE_FAILED          -15
#define RADIOLIB_ERR_SPI_READ_FAILED           -16
#define RADIOLIB_ERR_INVALID_ARGUMENT          -17
#define RADIOLIB_ERR_BUSY_TIMEOUT              -18
#define RADIOLIB_ERR_NOT_INITIALIZED           -19
#define RADIOLIB_ERR_UNSUPPORTED               -20
#define RADIOLIB_ERR_HEADER_DAMAGED             -21
#define RADIOLIB_ERR_INVALID_STATE              -22
#define RADIOLIB_ERR_RTTHREAD                   -23
#define RADIOLIB_LORA_DETECTED                  -24

#define RADIOLIB_NC                            (-1)

typedef void (*RadioLibIrqAction_t)(void *context);

#ifdef __cplusplus
}
#endif

#endif
