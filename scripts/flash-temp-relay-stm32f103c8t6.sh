#!/bin/bash
openocd                                         \
  -f ../../../apps/configs/interface/stlink.cfg \
  -f ../../../apps/configs/target/stm32f1x.cfg  \
  -c "program Build/temp-relay-stm32f103c8t6.hex verify reset exit"

# -c "set CPUTAPID 0x2ba01477"