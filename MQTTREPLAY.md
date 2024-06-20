# recording and replaying MQTT for development

## prerequisites

- Install [mqtt_recorder](https://github.com/rpdswtk/mqtt_recorder)

- MacOS: 
    Install mosquitto: `brew install mosquitto`
- configure MacOS moquitto for localhost only, no auth, TCP on port 1883, websockets on port 1884 like so:
- edit `/opt/homebrew/etc/mosquitto/mosquitto.conf`:

````
persistence false
log_dest file /opt/homebrew/var/log/mosquitto.log
include_dir /opt/homebrew/etc/mosquitto/conf.d
log_type all
````

- edit `/opt/homebrew/etc/mosquitto/default.conf`:
````
listener 1883 127.0.0.1
#listener 1883 0.0.0.0
protocol mqtt
allow_anonymous true
#connection_messages true


listener 1884 127.0.0.1
protocol websockets
allow_anonymous true
#connection_messages true
#websockets_log_level 255

````
-  restart mosquitto: `brew services restart mosquitto`


## recording a session via MQTT/TCP

- create a file `topics.json` with this content:
````json
{
    "topics": [
        "#"
    ]
}
````

- record from target device with:
` mqtt-recorder --host sensorbox.local --port 1883 --mode record  --file sensorbox.csv`

## replaying a recording in loop mode

- replay with `mqtt-recorder --loop 1 --host localhost --mode replay --file  sensorbox.csv`