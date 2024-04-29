#include <Arduino.h>
#include "sensors.hpp"
#include "i2cio.hpp"

#include <ICM_20948.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <PicoMQTT.h>
#include <ArduinoJson.h>
#include "timingpin.hpp"
#include "tickers.hpp"
#include "ublox.hpp"
#include "broker.hpp"
#include "baro.hpp"
#include "imu.hpp"
#include "RunningStats.hpp"
#include "meteo.hpp"

TICKER(baro, I2C_INTERVAL);
TICKER(gps, I2C_INTERVAL);
TICKER(imu, I2C_INTERVAL);
TICKER(stats, STATS_INTERVAL);

void sensor_loop(void) {
    if (TIME_FOR(baro)) {
        baro_loop();
        DONE_WITH(baro);
    }
    if (TIME_FOR(gps)) {
        ublox_loop();
        DONE_WITH(gps);
    }
    if (TIME_FOR(imu)) {
        imu_loop();
        DONE_WITH(imu);
    }
    if (TIME_FOR(stats)) {
        stats_loop();
        DONE_WITH(stats);
    }
}

void sensor_setup(void) {
    if (baro_setup()) {
        RUNTICKER(baro);
    }
    if (ublox_setup()) {
        RUNTICKER(gps);
    }
    if (imu_setup()) {
        RUNTICKER(imu);
    }
    stats_setup();
    RUNTICKER(stats);
}