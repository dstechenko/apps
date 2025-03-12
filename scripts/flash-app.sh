#!/bin/bash
west build $@ -p always -- -DOPENOCD=/opt/homebrew/bin/openocd -DOPENOCD_DEFAULT_PATH=/opt/homebrew/Cellar/open-ocd/0.12.0_1/share/openocd/scripts/ -DRPI_PICO_DEBUG_ADAPTER=cmsis-dap && \
west flash