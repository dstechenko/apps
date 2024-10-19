cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(temp-relay)

target_sources(app PRIVATE src/comms.c src/info.c src/monitor.c)