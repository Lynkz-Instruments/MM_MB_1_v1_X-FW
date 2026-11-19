#include "app_errors.h"
#include "app_settings.h"
#include "utils.h"

void Error_Handler(ErrorCode_t error_code)
{
    print("Error occurred: %d\r\n", error_code);
    print("Sending error code on port 66.\r\n");
    SendErrorCode(error_code);
    LM_Delay(500, 0);

    struct AppConfig_s current_config = app_get_device_config();
    EnterShutdown(current_config);
}
