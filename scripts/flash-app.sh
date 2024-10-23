#!/bin/bash
west build \
 -p always \
 -b rpi_pico/rp2040/w \
 -- -DOPENOCD=/opt/homebrew/bin/openocd -DOPENOCD_DEFAULT_PATH=/opt/homebrew/Cellar/open-ocd/0.12.0_1/share/openocd/scripts/ -DRPI_PICO_DEBUG_ADAPTER=cmsis-dap && \
west flash