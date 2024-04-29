#include <Arduino.h>
#include "i2cio.hpp"
#include <ICM_20948.h>
#include <ArduinoJson.h>
#include "timingpin.hpp"
#include "tickers.hpp"
#include "imu.hpp"
#include "broker.hpp"

static ICM_20948_I2C icm;

bool imu_setup( void) {
    if (detect(Wire, ICM_20948_I2C_ADDR_AD1)) {

        icm.begin(Wire, ICM_20948_I2C_ADDR_AD1);
        log_i("status: %s", icm.statusString());

        // icm.swReset();
        // log_i("status: %s", icm.statusString());
        delay(250);
        // icm.sleep(false);
        // log_i("status: %s", icm.statusString());

        // icm.lowPower(false);
        // log_i("status: %s %d", icm.statusString(), icm.status);
        return true;
        return (icm.status == ICM_20948_Stat_Ok) || (icm.status ==  ICM_20948_Stat_NoData);
    }
    return false;
}

void imu_loop( void) {
    if (icm.dataReady())   {
        icm.getAGMT();
        JsonDocument json;
        json["us"] = micros();

        json["ax"] = icm.accX();
        json["ay"] = icm.accY();
        json["az"] = icm.accZ();

        json["gx"] = icm.gyrX();
        json["gy"] = icm.gyrY();
        json["gz"] = icm.gyrZ();

        json["mx"] = icm.magX();
        json["my"] = icm.magY();
        json["mz"] = icm.magZ();

        auto publish = mqtt.begin_publish("imu", measureJson(json));
        serializeJson(json, publish);
        publish.send();
    }
}
