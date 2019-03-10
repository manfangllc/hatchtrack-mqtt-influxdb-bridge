# hatchtrack-mqtt-influxdb-bridge

## About

This program listens to a specific MQTT topic for any new messages. When a
message is received, it formats it and writes it to an InfluxDB instance that
stores all the data.

The general flow illustrated is as follows

```
MQTT Device --> MQTT Broker --> hatchtrack-mqtt-influxdb-bridge --> InfluxDB
```

The AWS IoT SDK is used for this program's MQTT client connection and libcurl
is used to make HTTPS POST requests to the InfluxDB instance.

## Compiling

Assuming you have a valid install of GCC, all that should be needed is to run
`make` at the top level directory. Note that this program is designed to run
on an x86_64 Linux machine, no effort has been made to allow this program to
run on any other architecture or operating system.
