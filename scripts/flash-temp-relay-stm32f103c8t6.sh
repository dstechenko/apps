#!/bin/bash
openocd -f ../../configs/board/st_nucleo_f4.cfg -c "program Build/temp-relay-f446re.hex verify reset exit"