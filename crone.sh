#!/bin/bash

# do not try reserving device (disable dbus)
export JACK_NO_AUDIO_RESERVATION=1

# start jack clients

supernova -u 57122 &
./build/crone/crone &

# start sclang (performs connections)
SCLANG=$(which sclang)
./build/ws-wrapper/ws-wrapper ws://*:5556 $SCLANG &
