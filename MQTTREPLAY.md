# recording and replaying MQTT for development

## prerequisites

- Install [mqtt_recorder](https://github.com/rpdswtk/mqtt_recorder)

- MacOS: 
    Install mosquitto: `brew install mosquitto`
    run mosquitto: `brew services start mosquitto`

## recording a session via MQTT/TCP

- create a file `topics.json` with this content:
````json
{
    "topics": [
        "#"
    ]
}
````

- record with:
` mqtt-recorder --host sensorbox.local --port 1883 --mode record  --file sensorbox.csv`

## replaying a recording

- start mosquitto on port 1883, no auth

- replay with `mqtt-recorder --loop 1 --host localhost --mode replay --file  sensorbox.csv`