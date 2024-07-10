#pragma once
#include <PicoMQTT.h>
#include <PicoWebsocket.h>

class CustomMQTTServer: public PicoMQTT::Server {
  protected:
    void on_connected(const char * client_id) override {
        log_i("client %s connected", client_id);
    }
    virtual void on_disconnected(const char * client_id) override {
        log_i("client %s disconnected", client_id);
    }
    virtual void on_subscribe(const char * client_id, const char * topic) override {
        log_i("client %s subscribed %s", client_id, topic);
    }
    virtual void on_unsubscribe(const char * client_id, const char * topic) override {
        log_i("client %s unsubscribed %s", client_id, topic);
    }
};
