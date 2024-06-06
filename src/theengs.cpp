#include <Arduino.h>
#include "freertos/ringbuf.h"
#include "ringbuffer.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#include "broker.hpp"
#include "util.hpp"
#include "fmicro.h"

#include "NimBLEDevice.h"
#include "decoder.h"

#ifndef BLE_ADV_QUEUELEN
    #define BLE_ADV_QUEUELEN 2048
#endif
// move queue to PSRAM if possible
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 1) && defined(BOARD_HAS_PSRAM)
    #define BLE_ADV_QUEUE_MEMTYPE MALLOC_CAP_SPIRAM
#else
    #define BLE_ADV_QUEUE_MEMTYPE MALLOC_CAP_INTERNAL
#endif

static NimBLEScan* pBLEScan;
static TheengsDecoder decoder;
static uint32_t scanTime = 3600 * 1000; // In seconds, 0 = scan forever
static espidf::RingBuffer *bleadv_queue;
static uint32_t queue_full, acquire_fail;

#ifdef NIMBLE_OLDAPI
class scanCallbacks: public  BLEAdvertisedDeviceCallbacks {
#else
class scanCallbacks: public  NimBLEScanCallbacks {
#endif

    std::string convertServiceData(std::string deviceServiceData) {
        int serviceDataLength = (int)deviceServiceData.length();
        char spr[2 * serviceDataLength + 1];
        for (int i = 0; i < serviceDataLength; i++) sprintf(spr + 2 * i, "%.2x", (unsigned char)deviceServiceData[i]);
        spr[2 * serviceDataLength] = 0;
        return spr;
    }

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        // Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
        JsonDocument doc;
        JsonObject BLEdata = doc.to<JsonObject>();
        String mac_adress = advertisedDevice->getAddress().toString().c_str();
        mac_adress.toUpperCase();
        BLEdata["id"] = (char*)mac_adress.c_str();

        if (advertisedDevice->haveName())
            BLEdata["name"] = (char*)advertisedDevice->getName().c_str();

        if (advertisedDevice->haveManufacturerData()) {
            char* manufacturerdata = BLEUtils::buildHexData(NULL, (uint8_t*)advertisedDevice->getManufacturerData().data(), advertisedDevice->getManufacturerData().length());
            BLEdata["manufacturerdata"] = manufacturerdata;
            free(manufacturerdata);
        }

        if (advertisedDevice->haveRSSI())
            BLEdata["rssi"] = (int)advertisedDevice->getRSSI();

        if (advertisedDevice->haveTXPower())
            BLEdata["txpower"] = (int8_t)advertisedDevice->getTXPower();

        if (advertisedDevice->haveServiceData()) {
            int serviceDataCount = advertisedDevice->getServiceDataCount();
            for (int j = 0; j < serviceDataCount; j++) {
                std::string service_data = convertServiceData(advertisedDevice->getServiceData(j));
                BLEdata["servicedata"] = (char*)service_data.c_str();
                std::string serviceDatauuid = advertisedDevice->getServiceDataUUID(j).toString();
                BLEdata["servicedatauuid"] = (char*)serviceDatauuid.c_str();
            }
        }
        BLEdata["time"] = fseconds();
        void *ble_adv = nullptr;
        size_t total = measureMsgPack(BLEdata);
        if (bleadv_queue->send_acquire((void **)&ble_adv, total, 0) != pdTRUE) {
            acquire_fail++;
            return;
        }
        size_t n = serializeMsgPack(BLEdata, ble_adv, total);
        if (n != total) {
            log_e("serializeMsgPack: expected %u got %u", total, n);
        } else {
            if ( bleadv_queue->send_complete(ble_adv) != pdTRUE) {
                queue_full++;
            }
        }
    }
};

void setup_ble(void) {
    bleadv_queue = new espidf::RingBuffer();
    bleadv_queue->create(BLE_ADV_QUEUELEN, RINGBUF_TYPE_NOSPLIT);

    // NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);
    // NimBLEDevice::setScanDuplicateCacheSize(200);

    NimBLEDevice::init("");

    pBLEScan = NimBLEDevice::getScan(); //create new scan
#ifdef NIMBLE_OLDAPI
    pBLEScan->setAdvertisedDeviceCallbacks(new scanCallbacks());
#else
    pBLEScan->setScanCallbacks(new scanCallbacks());
#endif
    pBLEScan->setActiveScan(false); // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
    pBLEScan->setWindow(37);  // How long to scan during the interval; in milliseconds.
    pBLEScan->setDuplicateFilter(false);
    pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.
#ifdef NIMBLE_OLDAPI
    pBLEScan->start(scanTime/1000, nullptr, false);
#else
    pBLEScan->start(scanTime, false);
#endif

}

void bleDeliver(JsonObject &BLEdata) {
    if (decoder.decodeBLEJson(BLEdata)) {
        BLEdata.remove("manufacturerdata");
        BLEdata.remove("servicedata");
        BLEdata.remove("servicedatauuid");
        BLEdata.remove("type");
        BLEdata.remove("cidc");
        BLEdata.remove("acts");
        BLEdata.remove("cont");
        BLEdata.remove("track");
        if (BLEdata.containsKey("volt")) {
            BLEdata["batt"] = volt2percent(BLEdata["volt"].as<float>());
        }
        NimBLEAddress mac = NimBLEAddress(BLEdata["id"].as<std::string>());
        auto publish = mqtt.begin_publish(bleTopic(mac), measureJson(BLEdata));
        serializeJson(BLEdata, publish);
        publish.send();
    }
}

void process_ble(void) {
    if (!pBLEScan->isScanning()) {
        pBLEScan->start(scanTime, false);
    }
    size_t size = 0;
    void *buffer = bleadv_queue->receive(&size, 0);
    if (buffer == nullptr) {
        return;
    }
    JsonDocument doc;
    JsonObject BLEdata = doc.to<JsonObject>();

    DeserializationError e = deserializeMsgPack(BLEdata, buffer, size);
    bleDeliver(BLEdata);
    bleadv_queue->return_item(buffer);
}
