
#ifdef M5UNIFIED
    #include <M5Unified.h>
#else
    #include <Arduino.h>
#endif

#include <Wire.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include "tickers.hpp"
#include "i2cio.hpp"
#ifdef DEM_SUPPORT
    #include "SD.h"
    #include "demlookup.hpp"
#endif
#include "sensor.hpp"
#include "params.hpp"
#include "settings.hpp"
#include "fmicro.h"
#include "pindefs.h"
//#include "broker.hpp"
#include <PicoMQTT.h>
extern PicoMQTT::Server mqtt;

#include <ESP32SvelteKit.h>
#include <LightMqttSettingsService.h>
#include <LightStateService.h>
#include <PsychicHttpServer.h>

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 120);

LightMqttSettingsService lightMqttSettingsService = LightMqttSettingsService(&server,
    esp32sveltekit.getFS(),
    esp32sveltekit.getSecurityManager());

LightStateService lightStateService = LightStateService(&server,
                                      esp32sveltekit.getSocket(),
                                      esp32sveltekit.getSecurityManager(),
                                      esp32sveltekit.getMqttClient(),
                                      &lightMqttSettingsService);

// #ifndef PSYCHIC_HTTP
// #include <PicoWebsocket.h>
// #endif
// class CustomMQTTServer: public PicoMQTT::Server {
//   protected:
//     void on_connected(const char * client_id) override {
//         log_i("client %s connected", client_id);
//     }
//     virtual void on_disconnected(const char * client_id) override {
//         log_i("client %s disconnected", client_id);
//     }
//     virtual void on_subscribe(const char * client_id, const char * topic) override {
//         log_i("client %s subscribed %s", client_id, topic);
//     }
//     virtual void on_unsubscribe(const char * client_id, const char * topic) override {
//         log_i("client %s unsubscribed %s", client_id, topic);
//     }
// };

// #if __has_include("myconfig.h")
//     #include "myconfig.h"
// #endif

// size_t getArduinoLoopTaskStackSize(void) {
//     return CUSTOM_ARDUINO_LOOP_STACK_SIZE;
// }
void sensor_setup(void);
void sensor_loop(void);
void irq_setup(void);
bool nfc_reader_present(void);
void battery_check(void);
void flow_report(bool force);
void broker_setup();
void broker_loop();

// #ifndef PSYCHIC_HTTP

// ::WiFiServer mqtt_tcp_server(MQTT_TCP);
// ::WiFiServer mqtt_ws_server(MQTT_WS);
// PicoWebsocket::Server<::WiFiServer> websocket_server(mqtt_ws_server);


// //CustomMQTTServer mqtt(mqtt_tcp_server);
// // CustomMQTTServer mqtt(mqtt_tcp_server, websocket_server);
// PicoMQTT::Server mqtt(mqtt_tcp_server, websocket_server);
// #else
// extern PicoMQTT::Server mqtt;
// #endif

TICKER(internal, INTERVAL);
TICKER(deadman, DEADMAN_INTERVAL);

extern battery_status_t battery_conf;

void setup() {

    delay(3000);
#ifdef M5UNIFIED
    auto cfg = M5.config();
    cfg.output_power = true;
    cfg.internal_imu = false;


    cfg.internal_rtc = false;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    M5.begin(cfg);
#if defined(ARDUINO_M5STACK_CORES3) || defined(ARDUINO_M5STACK_Core2)
    battery_conf.dev.device_present = true;
#endif
#endif
    Serial.begin(115200);
    // Serial.setDebugOutput(true);

    log_i("CPU: %s rev%d, CPU Freq: %d Mhz, %d core(s)", ESP.getChipModel(), ESP.getChipRevision(), getCpuFrequencyMhz(), ESP.getChipCores());
    log_i("Free heap: %d bytes", ESP.getFreeHeap());
    log_i("Free PSRAM: %d bytes", ESP.getPsramSize());
    log_i("SDK version: %s", ESP.getSdkVersion());

    RGBLED(64,0,0);


#if defined(ARDUINO_M5STACK_CORES3) ||  defined(ARDUINO_M5STACK_Core2)
    Wire.begin();
    Wire.setClock(I2C_100K);
#if defined(I2C1_SDA)
    Wire1.begin();
    Wire1.setClock(I2C_100K);
#endif
#if defined(DPS0_IRQ_PIN)
    pinMode(DPS0_IRQ_PIN, INPUT_PULLUP);
#endif
#if defined(DPS1_IRQ_PIN)
    pinMode(DPS1_IRQ_PIN, INPUT_PULLUP);
#endif
#if defined(DPS2_IRQ_PIN)
    pinMode(DPS2_IRQ_PIN, INPUT); // has external pulldown 33k
#endif

    // logic analyzer trace pins
    TRIGGER_SETUP(TRIGGER1);
    TRIGGER_SETUP(TRIGGER2);
    TRIGGER_SETUP(TRIGGER3);
    TRIGGER_SETUP(TRIGGER4);

    delay(10);
    i2c_scan(Wire);
#if defined(I2C1_SDA)
    i2c_scan(Wire1);
#endif

#endif

    // #if defined(ARDUINO_M5Stack_ATOMS3)
    //     Wire1.begin(I2C1_SDA, I2C1_SCL, I2C1_SPEED);
    //     i2c_scan(Wire1);
    // #endif

#if defined(DEVKITC) || defined(M5STAMP_C3U) || defined(ARDUINO_M5Stack_ATOMS3) || defined(ARDUINO_M5Stack_StampS3)
#if defined(I2C0_SDA)
    Wire.begin(I2C0_SDA, I2C0_SCL, I2C0_SPEED);
#endif
#if defined(I2C1_SDA)
    Wire1.begin(I2C1_SDA, I2C1_SCL, I2C1_SPEED);
#endif
#if DPS0_IRQ_PIN
    pinMode(DPS0_IRQ_PIN, INPUT_PULLUP);
#endif
#if DPS1_IRQ_PIN
    pinMode(DPS1_IRQ_PIN, INPUT);
#endif
#if DPS2_IRQ_PIN
    pinMode(DPS2_IRQ_PIN, INPUT_PULLUP);
#endif
#if IMU_IRQ_PIN
    pinMode(IMU_IRQ_PIN, INPUT_PULLUP);
#endif
#if defined(TRACE_PINS)
    TRIGGER_SETUP(TRIGGER1);
    TRIGGER_SETUP(TRIGGER2);
#endif
#if defined(I2C0_SDA)
    i2c_scan(Wire);
#endif
#if defined(I2C1_SDA)
    i2c_scan(Wire1);
#endif
#endif
    // start ESP32-SvelteKit
    esp32sveltekit.begin();

    setup_queues();
    settings_setup();
    broker_setup();

    // load the initial light settings
    lightStateService.begin();
    // start the light service
    lightMqttSettingsService.begin();


#ifdef DEM_SUPPORT
    dem_setup(SD, "/dem");
    printDems();
#endif
    sensor_setup();
    BaseType_t ret = run_baro_task();
    if (ret != pdPASS) {
        log_e("failed to create baro_task task: %d", ret);
    } else {
        log_i("baro_task task created");
    }
    mqtt.begin();

    mqtt.subscribe("system/interval", [](const char * topic, const char * payload) {
        uint32_t new_interval = strtoul (payload, NULL, 0);
        if (new_interval  > MIN_INTERVAL) {
            CHANGE_TICKER(internal, new_interval);
            log_i("changed ticker to %u ms", new_interval);
        }
    });
    mqtt.subscribe("system/reboot",  [](const char * topic, const char * payload) {
        ESP.restart();
    });

    mqtt.subscribe("baro/reinit",  [](const char * topic, const char * payload) {
        int n = atoi(payload);
        if (n < 0)
            return;
        if (n > 2)
            return;
        int16_t ret = dps368_setup(n);
        log_i("reinit %d: %d", n, ret);
    });

    RUNTICKER(internal);
    RUNTICKER(deadman);
}


void loop() {
    // TOGGLE(TRIGGER3);
    mqtt.loop();
    // TOGGLE(TRIGGER3);

    // TOGGLE(TRIGGER1);
    // webserver_loop();
    // TOGGLE(TRIGGER1);

    TOGGLE(TRIGGER2);
    sensor_loop();
    TOGGLE(TRIGGER2);

    if (TIME_FOR(internal)) {
        mqtt.publish("system/interval", String(internal_update_ms));
        mqtt.publish("system/free-heap", String(ESP.getFreeHeap()));
        mqtt.publish("system/psram-usage", String(ESP.getPsramSize() - ESP.getFreePsram()));
        mqtt.publish("system/reboot", "0");
        mqtt.publish("baro/reinit", "-1");
#ifdef NFC_SUPPORT
        mqtt.publish("nfc/reader", String(nfc_reader_present()));
#endif
#ifdef DEM_SUPPORT
        publishDems();
#endif
        if (battery_conf.dev.device_present) {
            battery_check();
        }
        settings_tick();
#if defined(FLOWSENSOR) || defined(QUADRATURE_DECODER)
        flow_report(true);
#endif
        DONE_WITH(internal);
    }
#ifdef M5UNIFIED
    // M5.update();
#endif
    yield();
}
