#pragma once


#define I2C_100K 100000
#define I2C_400K 400000

#define MEASMT_QUEUELEN 16384 // 2048
#define BAROTASK_STACKSIZE 6144
#define BAROTASK_PRIORITY 1
#define BAROTASK_CORE 1
#define PRESSURE_DELAY_MARGIN 0.01 // sec
#define TEMP_DELAY_MARGIN 0.005 // sec

#define TEMP_COUNT_MASK  7
#define INITIAL_ALTITUDE_VALUES 10

#define NAV_FREQUENCY 1
#define UBLOX_I2C_ADDR 0x42

#define NFC_USE_I2C
#define NFC_I2C_TIMEOUT 10
#ifndef MRFC522_I2C_ADDR
#define MRFC522_I2C_ADDR 0x28
#endif
#ifndef NFC_WIRE
#define NFC_WIRE Wire1
#endif

#define NFC_MAX_MSG_SIZE 4096
#define BW_MIMETYPE "application/balloonware"
#define BW_ALT_MIMETYPE "bw"


