/**
  ******************************************************************************
  * @file    lora_mod.c
  * @author  Alexandre Desgagné (alexd@lynkz.ca)
  * @brief   Control the radio modulation.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_app.h"
#include "lora_mod.h"
#include "radio.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "sys_conf.h"

/* Private define ------------------------------------------------------------*/
#define F_868MHz                      868000000
#define P_14dBm                       14
#define P_22dBm                       22
#define SF12                          12
#define CR4o5                         1
#define EMISSION_POWER                P_14dBm
#define CONTINUOUS_TIMEOUT            0xFFFF
#define LORA_PREAMBLE_LENGTH          8         /* Same for Tx and Rx */
#define LORA_SYMBOL_TIMEOUT           600       /* Symbols */
#define TX_TIMEOUT_VALUE              60000     /* 60s */
#define LORA_FIX_LENGTH_PAYLOAD_OFF   false
#define LORA_IQ_INVERSION_OFF         false
#define TX_RADIO_TONE                 (1<<0)
#define TX_RADIO_MODU                 (1<<2)
#define RX_RADIO_MODU                 (1<<3)
#define RX_CONTINUOUS_ON              1
//#define PRBS9_INIT                    ( ( uint16_t) 2 )
//#define DEFAULT_PAYLOAD_LEN           16
#define DEFAULT_LDR_OPT               2
#define DEFAULT_FSK_DEVIATION         25000
#define DEFAULT_GAUSS_BT              3 /*Lora default in legacy*/

/* Private variables ---------------------------------------------------------*/
static uint8_t RadioState = 0;

static ModParameter_t modParam = { TEST_LORA, F_868MHz, EMISSION_POWER, BW_125kHz, SF12, CR4o5, 0, 0, DEFAULT_FSK_DEVIATION, DEFAULT_LDR_OPT, DEFAULT_GAUSS_BT};

static __IO uint32_t RadioTxDone_flag = 0;
static __IO uint32_t RadioTxTimeout_flag = 0;
static __IO uint32_t RadioRxDone_flag = 0;
static __IO uint32_t RadioRxTimeout_flag = 0;
static __IO uint32_t RadioError_flag = 0;
static __IO int16_t last_rx_rssi = 0;
static __IO int8_t last_rx_LoraSnr_FskCfo = 0;

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * Radio test payload pointer
 */
static uint16_t last_payload_size = 0;
static uint8_t last_payload[256] = {0};

/* Private function prototypes -----------------------------------------------*/
/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void);

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError(void);


int32_t radio_set_config(ModParameter_t *Param)
{
  UTIL_MEM_cpy_8(&modParam, Param, sizeof(ModParameter_t));

  return 0;
}

int32_t radio_get_config(ModParameter_t *Param)
{
  UTIL_MEM_cpy_8(Param, &modParam, sizeof(ModParameter_t));
  return 0;
}

int32_t radio_tone(void)
{
  if ((RadioState & TX_RADIO_TONE) != TX_RADIO_TONE)
  {
    RadioState |= TX_RADIO_TONE;
    Radio.SetTxContinuousWave(modParam.freq, modParam.power, CONTINUOUS_TIMEOUT);
    return 0;
  }
  return -1;
}

int32_t radio_stop(void)
{
  RadioState = 0;
  Radio.Sleep();
  return 0;
}

int32_t radio_tx(uint8_t * buffer, unsigned size)
{
	TxConfigGeneric_t TxConfig;

	if (buffer == NULL){
		return -1;
	}

	if ((RadioState & TX_RADIO_MODU) != TX_RADIO_MODU)
	{
        RadioState |= TX_RADIO_MODU;

		// APP_TPRINTF("Radio Tx\r\n");

		// Radio initialization
		RadioEvents.TxDone = OnTxDone;
		RadioEvents.RxDone = OnRxDone;
		RadioEvents.TxTimeout = OnTxTimeout;
		RadioEvents.RxTimeout = OnRxTimeout;
		RadioEvents.RxError = OnRxError;
		Radio.Init(&RadioEvents);

		Radio.SetChannel(modParam.freq);

		if (modParam.modulation == TEST_FSK){
			// fsk modulation
			uint8_t syncword[] = { 0xC1, 0x94, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00 };
			TxConfig.fsk.ModulationShaping = (RADIO_FSK_ModShapings_t)((modParam.BTproduct == 0) ? 0 : modParam.BTproduct + 7);
			TxConfig.fsk.FrequencyDeviation = modParam.fskDev;
			TxConfig.fsk.BitRate = modParam.loraSf_datarate; /*BitRate*/
			TxConfig.fsk.PreambleLen = 3;   /*in Byte        */
			TxConfig.fsk.SyncWordLength = 3; /*in Byte        */
			TxConfig.fsk.SyncWord = syncword; /*SyncWord Buffer*/
			TxConfig.fsk.whiteSeed = 0x01FF; /*WhiteningSeed  */
			TxConfig.fsk.HeaderType = RADIO_FSK_PACKET_VARIABLE_LENGTH; /* If the header is explicit, it will be transmitted in the GFSK packet. If the header is implicit, it will not be transmitted*/
			TxConfig.fsk.CrcLength = RADIO_FSK_CRC_2_BYTES_CCIT;       /* Size of the CRC block in the GFSK packet*/
			TxConfig.fsk.CrcPolynomial = 0x1021;
			TxConfig.fsk.Whitening = RADIO_FSK_DC_FREE_OFF;
			Radio.RadioSetTxGenericConfig(GENERIC_FSK, &TxConfig, modParam.power, TX_TIMEOUT_VALUE);
		}
		else if (modParam.modulation == TEST_MSK){
			// msk modulation
			uint8_t syncword[] = { 0xC1, 0x94, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00 };
			TxConfig.msk.ModulationShaping = (RADIO_FSK_ModShapings_t)((modParam.BTproduct == 0) ? 0 : modParam.BTproduct + 7);
			TxConfig.msk.BitRate = modParam.loraSf_datarate; /*BitRate*/
			TxConfig.msk.PreambleLen = 3;   /*in Byte        */
			TxConfig.msk.SyncWordLength = 3; /*in Byte        */
			TxConfig.msk.SyncWord = syncword; /*SyncWord Buffer*/
			TxConfig.msk.whiteSeed = 0x01FF; /*WhiteningSeed  */
			TxConfig.msk.HeaderType = RADIO_FSK_PACKET_VARIABLE_LENGTH; /* If the header is explicit, it will be transmitted in the GFSK packet. If the header is implicit, it will not be transmitted*/
			TxConfig.msk.CrcLength = RADIO_FSK_CRC_2_BYTES_CCIT;       /* Size of the CRC block in the GFSK packet*/
			TxConfig.msk.CrcPolynomial = 0x1021;
			TxConfig.msk.Whitening = RADIO_FSK_DC_FREE_OFF;
			Radio.RadioSetTxGenericConfig(GENERIC_MSK, &TxConfig, modParam.power, TX_TIMEOUT_VALUE);
		}
		else if (modParam.modulation == TEST_LORA){
			/*lora modulation*/
			TxConfig.lora.Bandwidth = (RADIO_LoRaBandwidths_t) modParam.bandwidth;
			TxConfig.lora.SpreadingFactor = (RADIO_LoRaSpreadingFactors_t) modParam.loraSf_datarate; /*BitRate*/
			TxConfig.lora.Coderate = (RADIO_LoRaCodingRates_t)modParam.codingRate;
			TxConfig.lora.LowDatarateOptimize = (RADIO_Ld_Opt_t)modParam.lowDrOpt; /*0 inactive, 1 active, 2: auto*/
			TxConfig.lora.PreambleLen = LORA_PREAMBLE_LENGTH;
			TxConfig.lora.LengthMode = RADIO_LORA_PACKET_VARIABLE_LENGTH;
			TxConfig.lora.CrcMode = RADIO_LORA_CRC_ON;
			TxConfig.lora.IqInverted = RADIO_LORA_IQ_NORMAL;
			Radio.RadioSetTxGenericConfig(GENERIC_LORA, &TxConfig, modParam.power, TX_TIMEOUT_VALUE);
			Radio.SetPublicNetwork(false); /*set private syncword*/
		}
		else if (modParam.modulation == TEST_BPSK){
			TxConfig.bpsk.BitRate = modParam.loraSf_datarate; /*BitRate*/
			Radio.RadioSetTxGenericConfig(GENERIC_BPSK, &TxConfig, modParam.power, TX_TIMEOUT_VALUE);
		}
		else{
			return -1; /*error*/
		}

		// Send payload
		Radio.Send(buffer, size);

		// Wait Tx done/timeout
		UTIL_SEQ_WaitEvt(1 << CFG_SEQ_Evt_RadioOnTstRF);

		if (RadioTxDone_flag == 1){
			APP_TPRINTF("OnTxDone\r\n");
		}

		if (RadioTxTimeout_flag == 1){
			APP_TPRINTF("OnTxTimeout\r\n");
		}

		if (RadioError_flag == 1){
			APP_TPRINTF("OnTxError\r\n");
		}

		// Reset TX Done or timeout flags
		RadioTxDone_flag = 0;
		RadioTxTimeout_flag = 0;
		RadioError_flag = 0;

		RadioState &= ~TX_RADIO_MODU;
		return 0;
	}
	return -1;
}

int32_t radio_rx(int32_t rx_timeout_ms)
{
	RxConfigGeneric_t RxConfig = {0};

	if ((RadioState & RX_RADIO_MODU) != RX_RADIO_MODU){
        RadioState |= RX_RADIO_MODU;

		// Radio initialization
		RadioEvents.TxDone = OnTxDone;
		RadioEvents.RxDone = OnRxDone;
		RadioEvents.TxTimeout = OnTxTimeout;
		RadioEvents.RxTimeout = OnRxTimeout;
		RadioEvents.RxError = OnRxError;
		Radio.Init(&RadioEvents);

		// Rx config
		Radio.SetChannel(modParam.freq);

		if (modParam.modulation == TEST_FSK){
			// fsk modulation
			uint8_t syncword[] = { 0xC1, 0x94, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00 };
			RxConfig.fsk.ModulationShaping = (RADIO_FSK_ModShapings_t)((modParam.BTproduct == 0) ? 0 : modParam.BTproduct + 8);
			RxConfig.fsk.Bandwidth = modParam.bandwidth;
			RxConfig.fsk.BitRate = modParam.loraSf_datarate; /*BitRate*/
			RxConfig.fsk.PreambleLen = 3; /*in Byte*/
			RxConfig.fsk.SyncWordLength = 3; /*in Byte*/
			RxConfig.fsk.SyncWord = syncword; /*SyncWord Buffer*/
			RxConfig.fsk.PreambleMinDetect = RADIO_FSK_PREAMBLE_DETECTOR_08_BITS;
			RxConfig.fsk.whiteSeed = 0x01FF; /*WhiteningSeed*/
			RxConfig.fsk.LengthMode = RADIO_FSK_PACKET_VARIABLE_LENGTH; /* If the header is explicit, it will be transmitted in the GFSK packet. If the header is implicit, it will not be transmitted*/
			RxConfig.fsk.CrcLength = RADIO_FSK_CRC_2_BYTES_CCIT;       /* Size of the CRC block in the GFSK packet*/
			RxConfig.fsk.CrcPolynomial = 0x1021;
			RxConfig.fsk.Whitening = RADIO_FSK_DC_FREE_OFF;
			RxConfig.fsk.MaxPayloadLength = 255;
			RxConfig.fsk.AddrComp = RADIO_FSK_ADDRESSCOMP_FILT_OFF;
			Radio.RadioSetRxGenericConfig(GENERIC_FSK, &RxConfig, RX_CONTINUOUS_ON, 0);
		}
		else if (modParam.modulation == TEST_LORA){
			// Lora modulation
			RxConfig.lora.Bandwidth = (RADIO_LoRaBandwidths_t) modParam.bandwidth;
			RxConfig.lora.SpreadingFactor = (RADIO_LoRaSpreadingFactors_t) modParam.loraSf_datarate; /*BitRate*/
			RxConfig.lora.Coderate = (RADIO_LoRaCodingRates_t)modParam.codingRate;
			RxConfig.lora.LowDatarateOptimize = (RADIO_Ld_Opt_t)modParam.lowDrOpt; /*0 inactive, 1 active, 2: auto*/
			RxConfig.lora.PreambleLen = LORA_PREAMBLE_LENGTH;
			RxConfig.lora.LengthMode = RADIO_LORA_PACKET_VARIABLE_LENGTH;
			RxConfig.lora.CrcMode = RADIO_LORA_CRC_ON;
			RxConfig.lora.IqInverted = RADIO_LORA_IQ_NORMAL;
			Radio.RadioSetRxGenericConfig(GENERIC_LORA, &RxConfig, RX_CONTINUOUS_ON, LORA_SYMBOL_TIMEOUT);
			Radio.SetPublicNetwork(false); /*set private syncword*/
		}
		else{
			// excluding MSK Rx
			return -1; /* error */
		}

		if (modParam.lna == 0){
			Radio.Rx(rx_timeout_ms);
		}
		else{
			Radio.RxBoosted(rx_timeout_ms);
		}

		// Wait Rx done/timeout
		UTIL_SEQ_WaitEvt(1 << CFG_SEQ_Evt_RadioOnTstRF);

		if (RadioRxDone_flag == 1){
			int16_t rssi = last_rx_rssi;
			int8_t LoraSnr_FskCfo = last_rx_LoraSnr_FskCfo;
			APP_TPRINTF("OnRxDone\r\n");
			APP_PRINTF("+EVT:RX:%d:", last_payload_size);
			for (uint16_t i = 0; i < last_payload_size; ++i){
				APP_PRINTF("%02X",(int)last_payload[i]);
			}
			APP_PRINTF("\r\n");
			APP_PRINTF("+EVT:%d:%d\r\n", rssi, LoraSnr_FskCfo); // dbm & db
		}

		if (RadioRxTimeout_flag == 1){
			APP_TPRINTF("OnRxTimeout\r\n");
		}

		if (RadioError_flag == 1){
			APP_TPRINTF("OnRxError\r\n");
		}

		  // Reset flags
		RadioRxDone_flag = 0;
		RadioRxTimeout_flag = 0;
		RadioError_flag = 0;

		RadioState &= ~RX_RADIO_MODU;
	    return 0;
	}
	else{
		return -1;
	}
}

/* Private Functions Definition -----------------------------------------------*/
void OnTxDone(void)
{
  // Set TxDone flag
  RadioTxDone_flag = 1;
  UTIL_SEQ_SetEvt(1 << CFG_SEQ_Evt_RadioOnTstRF);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t LoraSnr_FskCfo)
{
  last_rx_rssi = rssi;
  last_rx_LoraSnr_FskCfo = LoraSnr_FskCfo;
  last_payload_size = size;
  UTIL_MEM_cpy_8(last_payload, payload, size);

  // Set Rxdone flag
  RadioRxDone_flag = 1;
  UTIL_SEQ_SetEvt(1 << CFG_SEQ_Evt_RadioOnTstRF);
}

void OnTxTimeout(void)
{
  // Set timeout flag
  RadioTxTimeout_flag = 1;
  UTIL_SEQ_SetEvt(1 << CFG_SEQ_Evt_RadioOnTstRF);
}

void OnRxTimeout(void)
{
  // Set timeout flag
  RadioRxTimeout_flag = 1;
  UTIL_SEQ_SetEvt(1 << CFG_SEQ_Evt_RadioOnTstRF);
}

void OnRxError(void)
{
  // Set error flag
  RadioError_flag = 1;
  UTIL_SEQ_SetEvt(1 << CFG_SEQ_Evt_RadioOnTstRF);
}
