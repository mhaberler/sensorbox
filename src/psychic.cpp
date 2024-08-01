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

::WiFiServer tcp_server(1883);
PicoMQTT::Server mqtt(tcp_server, websocket_handler);

void webserver_setup() {

    server.config.max_uri_handlers = 20;

    Serial.println("Connecting...");

    WiFi.begin();
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    MDNS.begin("picomqtt");

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
    server.listen(443, server_cert.c_str(), server_key.c_str());
#else
    server.listen(80);
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
    // if (millis() - last > 1000) {
    //     last = millis();
    //     char buf[100];
    //     itoa(counter++, buf, 10);
    //     mqtt.publish("counter", buf );
    //     itoa(ESP.getFreeHeap(), buf, 10);
    //     mqtt.publish("freeheap", buf );
    //     itoa(ESP.getPsramSize() - ESP.getFreePsram(), buf, 10);
    //     mqtt.publish("psramusage", buf );
    // }
}


#endif
