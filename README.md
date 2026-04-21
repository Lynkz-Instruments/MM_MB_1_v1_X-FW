# MM_MB_1_v1_X-FW
Firmware for LipSensor v1.0

- RegionUS915.h has settings for bands and channels. If US915_MAX_NB_CHANNELS is changed to 8, join succeeds but stuck in __WFI.

-> lorawan_end_node has custom user code not explicit from ioc. added the equivalent to MM_MB_1 and modified accordingly for our purpose.