
#include <LittleFS.h>

#include <PicoMQTT.h>
#include <PsychicHttp.h>

// #define CONFIG_ESP_HTTPS_SERVER_ENABLE 1
#include <PicoMQTT.h>
#include <PsychicHttpsServer.h>
#include <PsychicWebSocketProxy.h>

// define a PsychicWebSocketProxy::Server object, which will connect
// the async PsychicHTTP server with the synchronous PicoMQTT library.
PsychicWebSocketProxy::Server websocket_handler;

// Initialize a PicoMQTT::Server using the PsychicWebSocketProxy::Server
// object as the server to use.
PicoMQTT::Server mqtt(websocket_handler);

String server_cert;
String server_key;
PsychicHttpsServer server;

void broker_setup() {
    server.config.max_uri_handlers = 20;
    server.ssl_config.httpd.max_open_sockets = 3;
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

    // Some strict clients require that the websocket subprotocol is mqtt.
    // NOTE: The subprotocol must be set *before* attaching the handler to a
    // server path using server.on(...)
    websocket_handler.setSubprotocol("mqtt");

    server.listen(443, server_cert.c_str(), server_key.c_str());

    // bind the PsychicWebSocketProxy::Server to an url like a websocket handler
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

void broker_loop() {
    mqtt.loop();
}

