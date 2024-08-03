#ifdef PSYCHIC_HTTP
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "Esp.h"

#include <PicoMQTT.h>
#include <PsychicHttp.h>

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    #include <LittleFS.h>
    #include <PsychicHttpsServer.h>
#endif


#include "server.h"
#include "shifting_buffer_proxy.h"

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    String server_cert;
    String server_key;
    PsychicHttpsServer server;
#else
    PsychicHttpServer server;
#endif

PsychicWebSocketProxy::Server websocket_handler([] { return new PsychicWebSocketProxy::ShiftingBufferProxy<1024>(); });

::WiFiServer tcp_server(MQTT_TCP);
PicoMQTT::Server mqtt(tcp_server, websocket_handler);
static MDNSResponder *mdns_responder;

void webserver_setup() {

    server.config.max_uri_handlers = 20;

    Serial.println("Connecting...");

    WiFi.begin();
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    // MDNS.begin("sensorbox");
    if (!mdns_responder) {
        mdns_responder = new MDNSResponder();
    }
    if (mdns_responder->begin(HOSTNAME)) {
        log_i("MDNS responder started");
        mdns_responder->addService("http", "tcp", HTTP_PORT);
        mdns_responder->addService("https", "tcp", HTTPS_PORT);
#ifdef MQTT_TCP
        mdns_responder->addService("mqtt", "tcp", MQTT_TCP);
#endif
#ifdef MQTT_WS
        mdns_responder->addService("mqtt-ws", "tcp", MQTT_WS);
        mdns_responder->addService("mqtt-wss", "tcp", HTTPS_PORT);
#endif
        // mdns_responder->enableWorkstation(ESP_IF_WIFI_STA);
    }
    Serial.println(WiFi.localIP());

    {
        LittleFS.begin();
        File fp = LittleFS.open("/server.crt");
        if (fp) {
            server_cert = fp.readString();
            fp.close();
        }

        fp = LittleFS.open("/server.key");
        if (fp) {
            server_key = fp.readString();
            fp.close();
        }
    }
    log_e("crt='%s' key='%s'", server_cert.c_str(), server_key.c_str());
#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    server.listen(HTTPS_PORT, server_cert.c_str(), server_key.c_str());
#else
    server.listen(HTTP_PORT);
#endif

    websocket_handler.setSubprotocol("mqtt");

    server.on("/mqtt", &websocket_handler);
    server.on("/hello", [](PsychicRequest * request) {
        return request->reply(200, "text/plain", "Hello world!");
    });

    // Subscribe to a topic and attach a callback
    mqtt.subscribe("picomqtt/#", [](const char * topic, const char * payload) {
        // payload might be binary, but PicoMQTT guarantees that it's zero-terminated
        Serial.printf("Received message in topic '%s': %s\n", topic, payload);
    });

    mqtt.begin();
}

uint32_t last, counter;

void webserver_loop() {
    mqtt.loop();
}


#endif
