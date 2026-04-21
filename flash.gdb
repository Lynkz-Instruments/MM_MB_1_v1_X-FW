#gdb-multiarch --quiet -f build/Debug/MM_MB_1_v1_X-FW.elf -x flash.gdb
set confirm off
set pagination off
set verbose on
target extended-remote /dev/ttyACM0
monitor tpwr disable
shell sleep 2
monitor tpwr enable
monitor reset init
monitor swd_scan
attach 1
load
monitor reset run
exit