#include "app_errors.h"
#include "app_settings.h"
#include "utils.h"

void Error_Handler(ErrorCode_t error_code)
{
    print("Error occurred: %d\r\n", error_code);
    print("Sending error code on port 66.\r\n");
    SendErrorCode(error_code);
    
    // Entering shutdown mode based on current mode.
    AppMode_t current_mode = app_get_device_mode();
    struct AppConfig_s current_config = app_get_device_config();
    if (current_mode == APP_MODE_OPERATION){
        EnterShutdownNoBMA(current_config);
    }
    else{
        EnterShutdownWithBMA(current_config);
    }
}
