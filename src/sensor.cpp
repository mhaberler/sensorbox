#include <Arduino.h>
#include "sensor.hpp"
#include "i2cio.hpp"
#include "scanner.hpp"


#include <ICM_20948.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <PicoMQTT.h>
#include <ArduinoJson.h>
#include "timingpin.hpp"
#include "tickers.hpp"
#include "broker.hpp"
#include "RunningStats.hpp"
#include "meteo.hpp"

extern EventGroupHandle_t eventGroup;

TICKER(gps, GPS_INTERVAL);
TICKER(baro, BARO_INTERVAL);
TICKER(stats, STATS_INTERVAL);
TICKER(ble, BLE_INTERVAL);
TICKER(nfc, NFC_INTERVAL);
TICKER(imu, IMU_INTERVAL);

void stats_loop(void);
void stats_setup(void);
void nfc_setup(void);
void nfc_loop(void);
void nfc_poll(void);
void flow_report(bool force);
void flow_setup(void);
void baro_loop(void);

void sensor_loop(void) {

#if defined(FLOWSENSOR) || defined(QUADRATURE_DECODER)
    flow_report(false);
#endif
    if (TIME_FOR(baro)) {
        xEventGroupSetBits(eventGroup, EVENT_TRIGGER_DPS368);
        DONE_WITH(baro);
    }
    if (TIME_FOR(imu)) {
        xEventGroupSetBits(eventGroup, EVENT_TRIGGER_ICM_20948);
        // imu_loop();
        DONE_WITH(imu);
    }
#ifdef UBLOX_SUPPORT
    if (TIME_FOR(gps)) {
        // ublox_trigger_read();
        xEventGroupSetBits(eventGroup, EVENT_TRIGGER_NEO_M9N);
        DONE_WITH(gps);
    }
#endif
    if (TIME_FOR(nfc)) {
        xEventGroupSetBits(eventGroup, EVENT_TRIGGER_NFC);

        // nfc_loop();
        DONE_WITH(nfc);
    }
    if (TIME_FOR(ble)) {
        process_ble();
        DONE_WITH(ble);
    }
    if (TIME_FOR(stats)) {
        stats_loop();
        DONE_WITH(stats);
    }
}

void sensor_setup(void) {
#ifdef BLE_SUPPORT
    setup_ble();
    RUNTICKER(ble);
#endif

    // for (auto i = 0; i < num_dps_sensors; i++) {
    //     int n = 3;
    //     int16_t ret;
    //     while (n--) {
    //         ret = dps368_setup(i);
    //         if (ret == DPS__SUCCEEDED) {
    //             break;
    //         }
    //         log_e("dps%d setup failed ret=%d - retrying", i, ret);
    //         delay(200);
    //     }
    //     if (ret != DPS__SUCCEEDED) {
    //         log_e("dps%d setup failed ret=%d - giving up", i, ret);
    //     }
    // }
    // RUNTICKER(baro);

#ifdef UBLOX_SUPPORT
    if (ublox_setup()) {
        log_i("ublox initialized");
        RUNTICKER(gps);
    }
#endif
#ifdef IMU_SUPPORT
    if (imu_setup(&imu_sensor)) {
        log_i("imu initialized");
    }
    RUNTICKER(imu);
#endif
#if defined(FLOWSENSOR) || defined(QUADRATURE_DECODER)
    flow_setup();
#endif

#ifdef NFC_SUPPORT
    nfc_setup();
    RUNTICKER(nfc);
#endif
    stats_setup();
    RUNTICKER(stats);
}