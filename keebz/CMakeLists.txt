cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(keebz)

target_sources(app PRIVATE src/keebz_usb_hid.c src/keebz_usb_dev.c)