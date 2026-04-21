set confirm off
set pagination off
set verbose off
file .pio/build/nrf52840/firmware.elf
target extended-remote /dev/ttyACM0
monitor tpwr disable
monitor tpwr enable
monitor swd_scan
exit


monitor tpwr disable
>shell sleep 1.0
>monitor tpwr enable
>monitor swd_scan
>end