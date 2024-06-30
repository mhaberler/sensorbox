#include <esp-fs-webserver.h>  // https://github.com/cotestatnt/esp-fs-webserver
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP udp;
NTPClient timeClient (udp, NTP_POOL);

int64_t timeSinceEpoch;
int64_t millis_offset;


void ntp_setup() {
    timeClient.forceUpdate();
    timeSinceEpoch = ((int64_t)timeClient.getEpochTime ()) * 1000;
    millis_offset = timeSinceEpoch - millis();
    timeClient.end();
    time_t tmp = millis_offset / 1000;
    log_i("NTP reftime=%s", ctime (&tmp));
}