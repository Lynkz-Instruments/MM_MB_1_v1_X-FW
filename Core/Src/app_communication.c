/**
 * @file app_communication.c
 * @author Alexandre Desgagné (alexd@lynkz.ca)
 * @brief 
 * @version 0.1
 * @date 2026-02-16
 * 
 * @copyright Copyright (c) Lynkz Instruments Inc, Amos 2026
 * 
 */

#include "app_communication.h"

#include "utils.h"

// 0 -> No log
// 1 -> Error only
// 2 -> Error and info
#define APP_COMMUNICATION_VERBOSE (2)

// Common commands
#define SET_CONFIG_COMMMAND       (0xA1)
#define SET_DEVICE_MODE_COMMAND   (0xA2)

#define OK_RESPONSE               (0xE0)
#define DONE_RESPONSE             (0xE1)
#define FAILED_RESPONSE           (0xE2)
#define EMPTY_RESPONSE            (0xE3)

#define MAX_COMMAND_BYTE_COUNT    (32)

static void app_comm_process(uint8_t cmd, uint8_t const* data, uint16_t len);
static void app_comm_set_config(uint8_t * command);
static void app_comm_set_device_mode(uint8_t * command);

void app_comm_lora_process(uint8_t port, uint8_t * data, uint8_t len)
{
  #if APP_COMMUNICATION_VERBOSE >= 2
  print("Downlink received.\r\n");

  printf("RX: port=%d size=%d data=", port, len);
  for (uint8_t i = 0; i < len; i++)
  {
    printf("%02X", data[i]);
    if (i + 1 < len)
    {
      printf(" ");
    }
  }
  printf("\r\n");
  #endif

  app_comm_process(port, data, len);
}

static void app_comm_process(uint8_t cmd, uint8_t const* data, uint16_t len)
{
  if(data == NULL){return;}

  // if (len > NUS_COMMAND_MAX_COUNT_BYTES){
  //   #if APP_COMMUNICATION_VERBOSE >= 1
  //   NRF_LOG_ERROR("Invalid command.");
  //   #endif
  //   return;
  // }

  char command[MAX_COMMAND_BYTE_COUNT] = {0};
  memcpy(command, data, len);

  switch(cmd){
    case SET_CONFIG_COMMMAND:
        app_comm_set_config((uint8_t *)command);
        break;
    case SET_DEVICE_MODE_COMMAND:
        app_comm_set_device_mode((uint8_t *)command);
        break;
    default:
        break;
    }
}

static void app_comm_set_config(uint8_t * command)
{
  if (command == NULL){return;}

  #if APP_COMMUNICATION_VERBOSE >= 2
  print("Setting configuration.\r\n");
  #endif
  
  struct AppConfig_s cfg;
  memcpy(&cfg, &command[0], sizeof(struct AppConfig_s));
  app_set_device_config(cfg);
}

static void app_comm_set_device_mode(uint8_t * command)
{
  if (command == NULL){return;}
  
  #if APP_COMMUNICATION_VERBOSE >= 2
  print("Setting device mode.\r\n");
  #endif

  app_set_device_mode((AppMode_t)command[0]);
}
