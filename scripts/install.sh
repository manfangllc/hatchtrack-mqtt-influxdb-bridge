#!/bin/sh

DIR=$(realpath $(dirname "$0"))

cd $DIR
sudo mkdir -v -p /usr/local/share/hatchtrack-mqtt-influxdb-bridge-certificates
sudo cp -v ../certs/* /usr/local/share/hatchtrack-mqtt-influxdb-bridge-certificates
sudo cp -v ../build/bin/hatchtrack-mqtt-influxdb-bridge /usr/local/bin
sudo cp -v ./hatchtrack-mqtt-influxdb-bridge.service /etc/systemd/system/
