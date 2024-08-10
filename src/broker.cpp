
#include <LittleFS.h>

#include <PicoMQTT.h>
#include <PsychicHttp.h>

#define CONFIG_ESP_HTTPS_SERVER_ENABLE 1
#include <PsychicHttpsServer.h>

#include "server.h"
#include "shifting_buffer_proxy.h"

String server_cert;
String server_key;
PsychicHttpsServer httpsServer;

PsychicWebSocketProxy::Server websocket_handler([] { return new PsychicWebSocketProxy::ShiftingBufferProxy<1024>(); });

::WiFiServer tcp_server(1883);
PicoMQTT::Server mqtt(tcp_server, websocket_handler);

void broker_setup() {
    httpsServer.config.max_uri_handlers = 20;  
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
    // log_e("crt='%s' key='%s'", server_cert.c_str(), server_key.c_str());
    httpsServer.listen(443, server_cert.c_str(), server_key.c_str());

    websocket_handler.setSubprotocol("mqtt");

    httpsServer.on("/mqtt", &websocket_handler);
    httpsServer.on("/hello", [](PsychicRequest * request) {
        return request->reply(200, "text/plain", "Hello world!");
    });

    // Subscribe to a topic and attach a callback
    mqtt.subscribe("picomqtt/#", [](const char * topic, const char * payload) {
        // payload might be binary, but PicoMQTT guarantees that it's zero-terminated
        Serial.printf("Received message in topic '%s': %s\n", topic, payload);
    });

    mqtt.begin();
}

void broker_loop() {
    mqtt.loop();
}

