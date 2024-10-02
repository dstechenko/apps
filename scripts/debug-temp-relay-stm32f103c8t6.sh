#!/bin/bash
DIR=$(dirname "$0")
$DIR/../tools/cc/bin/*-gdb \
    -ex 'target ext :3333' \
    -ex 'symbol-file Build/temp-relay-stm32f103c8t6.elf'
