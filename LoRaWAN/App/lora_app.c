/* USER CODE BEGIN Header */
/**
	******************************************************************************
	* @file    lora_app.c
	* @author  MCD Application Team
	* @brief   Application of the LRWAN Middleware
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "adc_if.h"
#include "CayenneLpp.h"
#include "flash_if.h"

/* USER CODE BEGIN Includes */
#include "Region.h"
#include "app_eeprom.h"
#include "app_settings.h"
#include "app_accel.h"
#include "LoRaMac.h"
#include "utils.h"
#include "app_communication.h"
#include "gpio.h"
#include "lora_mod.h"
#include "stm32_systime.h"
#include "rtc.h"
#include "lynkz_key_gen.h"
#include "iwdg.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/**
	* @brief LoRa State Machine states
	*/
typedef enum TxEventType_e
{
	/**
	* @brief Appdata Transmission issue based on timer every TxDutyCycleTime
	*/
	TX_ON_TIMER,
	/**
	* @brief Appdata Transmission external event plugged on OnSendEvent( )
	*/
	TX_ON_EVENT
	/* USER CODE BEGIN TxEventType_t */

	/* USER CODE END TxEventType_t */
} TxEventType_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

#define DOWNLINK_WAIT_INTERVAL 2500 // ms, time to wait for downlink after sending uplink
#define LMHANDLER_BUSY_MAX_WAIT (DOWNLINK_WAIT_INTERVAL * 4) // ms, safety cap while draining a multi-NbTrans send (see SendTxData)
#define CALIBRATION_INTERVAL_MS 3000 // ms, fixed cadence for APP_MODE_CALIBRATION's live accel uplink

/* IWDG monitor (see IwdgCheckTimer/OnIwdgCheckTimerEvent). hiwdg's own
 * hardware timeout (~25.6s, see MX_IWDG_Init) is far shorter than any
 * legitimate sleep_time_minutes/idle_sleep_time_minutes (minutes to days),
 * so simply refreshing on a fixed period would keep it fed forever even if
 * the app were genuinely stuck - it must only be refreshed as long as the
 * app can show *recent evidence of real progress* (IwdgMarkProgress, called
 * from every RunScheduledAction/RunMotionStatusAction/RunCalibrationAction/
 * RF_TEST loop iteration that completes normally). That evidence's own
 * staleness allowance has to scale with whatever period actually governs
 * the current schedule, for the same reason - otherwise a device configured
 * for a long cadence would get reset mid-sleep for behaving exactly as
 * configured. CALIBRATION/RF_TEST aren't schedule-driven (fixed short
 * internal cadences of their own), so they get a fixed margin instead.
 * IWDG_CHECK_INTERVAL_MS must stay well under hiwdg's hardware timeout, to
 * leave retries within the window if one check is delayed. */
#define IWDG_CHECK_INTERVAL_MS            15000u
#define IWDG_PROGRESS_MARGIN_MULTIPLIER    2u
#define IWDG_PROGRESS_MIN_MARGIN_MINUTES   10u
#define IWDG_PROGRESS_FIXED_MARGIN_MINUTES 2u
/* Generous allowance covering boot/join before the first real task
 * completes (LoRaWAN join retries can legitimately take a while). */
#define IWDG_PROGRESS_BOOT_MARGIN_MINUTES  10u

/* GPS time does not observe leap seconds; UTC does. LoRaMac.c's DeviceTimeAns
 * handling (SRV_MAC_DEVICE_TIME_ANS) converts GPS time to Unix time via a
 * fixed GPS->Unix epoch offset only (UNIX_GPS_EPOCH_OFFSET), so the SysTime
 * it applies is fast by however many leap seconds have accumulated since
 * 1980-01-06. As of the last leap second insertion (2016-12-31 23:59:60 UTC),
 * that is 18s. Update this if IERS ever announces another one:
 * https://www.ietf.org/timezones/data/leap-seconds.list */
#define GPS_UTC_LEAP_SECONDS 18

/* Sanity floor for SysTime: if the RTC backup domain lost power (e.g. VBAT
 * tied to VDD and dropping on reset), SysTimeGet() reads back near the Unix
 * epoch instead of persisting across reboots. Anything before this is
 * treated as "never set this boot" rather than a real timestamp. Update
 * occasionally so it stays in the past. */
#define SYSTIME_SANITY_FLOOR_SECONDS 1735689600UL /* 2025-01-01 00:00:00 UTC */

/* DeviceTimeReq/DeviceTimeAns resync period - how often the node asks the
 * network for an absolute-time correction. There is no RTC smooth-
 * calibration/rate-learning mechanism in this codebase (removed - frequent
 * absolute resync bounds accumulated drift instead of trying to measure and
 * correct the crystal's rate). Hardcoded rather than EEPROM/downlink
 * configurable: this is an engineering tuning knob under active
 * experimentation, not a field setting - change and reflash directly.
 * See SyncDeviceTimeIfDue(): a device whose own measurement schedule
 * (sleep_time_minutes) is already shorter than this just resyncs on this
 * period; one whose schedule is longer resyncs at every scheduled wake
 * instead, since it would otherwise go longer than this period between
 * corrections. */
#define DEVICE_TIME_SYNC_PERIOD_MINUTES 30


/* how long the RF test tone stays on before the config uplink, in RF_TEST mode */
#define RF_TEST_TONE_DURATION_MS 20000

/* Every device on the same sleep_time_minutes/schedule_offset_minutes wakes
 * at the identical wall-clock-aligned instant (see ScheduleAction) - great
 * for cross-sensor comparison, but it also means every uplink in a cycle
 * would key up at the same moment across all of them. A small, per-device,
 * DevEUI-derived delay before that cycle's work starts spreads transmissions
 * out to avoid collisions, while staying negligible against multi-minute
 * scheduling cadences. */
#define DEV_EUI_OFFSET_SLOT_MS   400
#define DEV_EUI_OFFSET_SLOT_COUNT 16


/**
	* Join switch period value of the timer in ms
	*/
#define JOIN_TIME 2000

/*---------------------------------------------------------------------------*/
/*                             LoRaWAN NVM configuration                     */
/*---------------------------------------------------------------------------*/
/**
	* @brief LoRaWAN NVM Flash address
	* @note last 2 sector of a 128kBytes device
	*/
#define LORAWAN_NVM_BASE_ADDRESS                    ((void *)0x0803F000UL)

/* USER CODE BEGIN PD */
static const char *slotStrings[] = { "1", "2", "C", "C_MC", "P", "P_MC" };

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

/* USER CODE END PM */

/* Private function prototypes -----------------------------------------------*/
/**
	* @brief  LoRa End Node send request
	*/

/**
	* @brief  TX timer callback function
	* @param  context ptr of timer context
	*/
static void OnTxTimerEvent(void *context);

/**
	* @brief  join event callback function
	* @param  joinParams status of join
	*/
static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);

/**
	* @brief callback when LoRaWAN application has sent a frame
	* @brief  tx event callback function
	* @param  params status of last Tx
	*/
static void OnTxData(LmHandlerTxParams_t *params);

/**
	* @brief callback when LoRaWAN application has received a frame
	* @param appData data received in the last Rx
	* @param params status of last Rx
	*/
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

/**
	* @brief callback when LoRaWAN Beacon status is updated
	* @param params status of Last Beacon
	*/
static void OnBeaconStatusChange(LmHandlerBeaconParams_t *params);

/**
	* @brief callback when system time has been updated
	*/
static void OnSysTimeUpdate(void);

/**
	* @brief  Queue a DeviceTimeReq if the configured resync period has elapsed
	*         since the last successful DeviceTimeAns. The request piggybacks
	*         on the next uplink; no dedicated uplink is generated here.
	* @return true if a resync was actually queued this cycle.
	*/
static bool SyncDeviceTimeIfDue(void);

/**
	* @brief  Milliseconds from now until the next wall-clock boundary aligned
	*         to period_minutes, phase-shifted by offset_minutes past midnight
	*         UTC (e.g. period=90, offset=120 wakes at 02:00/03:30/05:00/...).
	*         Falls back to a plain period if SysTime looks unset (no sync has
	*         landed yet this boot).
	*/
static uint32_t MsUntilNextAlignedWake(uint16_t period_minutes, uint16_t offset_minutes);

/**
	* @brief  ScheduledActionTimer callback: schedules RunScheduledAction() via
	*         the sequencer (never do real work in timer ISR context), and on
	*         its first firing switches itself from one-shot (the aligned
	*         first wake) to a plain recurring period.
	*/
static void OnScheduledActionTimerEvent(void *context);

/**
	* @brief  IwdgCheckTimer callback. Deliberately does real work directly in
	*         ISR context (HAL_IWDG_Refresh()), unlike every other timer
	*         callback here - it must not depend on UTIL_SEQ_Run() ever
	*         getting back to idle, since a hung function parked in the
	*         sequencer's call stack would otherwise silently defeat it too.
	*         Refreshes hiwdg only if IwdgMarkProgress was called recently
	*         enough (within s_iwdg_stale_margin_s) - otherwise skips the
	*         refresh and lets hiwdg's own hardware timeout reset the device.
	*/
static void OnIwdgCheckTimerEvent(void *context);

/**
	* @brief  Records that a normal cycle just completed, and how long a
	*         silence after it is still normal (scaled to whatever cadence
	*         governs mode/config - 2x sleep_time_minutes/
	*         idle_sleep_time_minutes for the schedule-driven modes, a fixed
	*         short margin for CALIBRATION/RF_TEST's own fixed cadences).
	*         Called from every RunScheduledAction/RunMotionStatusAction/
	*         RunCalibrationAction/RF_TEST loop iteration that completes.
	*/
static void IwdgMarkProgress(AppMode_t mode, struct AppConfig_s config);

/**
	* @brief  RF_TEST mode's tone/config-uplink busy loop, extracted unchanged
	*         from the previous one-shot main() flow. Blocks until the mode is
	*         changed away from RF_TEST via downlink.
	*/
static void RunRfTestLoop(void);

/**
	* @brief Arms/disarms the BMA400 activity interrupt to match the given
	*        mode - enabled continuously in APP_MODE_BEACON (every movement
	*        should ping), disabled otherwise.
	*/
static void SyncActivityInterruptEnable(AppMode_t mode);

/**
	* @brief Mirrors SyncActivityInterruptEnable for GEN1 (no-motion): enabled
	*        in APP_MODE_BEACON while the last-reported state isn't already
	*        "stopped" (waiting for the next stillness transition). Disabled
	*        once that transition is confirmed (GEN1 re-asserts repeatedly
	*        while still, so it must be disarmed once that transition has been
	*        handled once), and in every other mode.
	*/
static void SyncNoMotionInterruptEnable(AppMode_t mode);

/**
	* @brief CalibrationTimer callback: schedules RunCalibrationAction() via
	*        the sequencer (never do real work in timer ISR context).
	*/
static void OnCalibrationTimerEvent(void *context);

/**
	* @brief Starts/stops the fixed-interval CalibrationTimer to match the
	*        given mode - running only in APP_MODE_CALIBRATION, stopped
	*        otherwise. Unlike the activity/no-motion interrupts this isn't a
	*        BMA400 interrupt at all, just a plain periodic uplink.
	*/
static void SyncCalibrationTimerEnable(AppMode_t mode);

/**
	* @brief Re-syncs every piece of mode-dependent device configuration
	*        (GEN1, GEN2, the calibration timer) to match the given mode in
	*        one place, so a mode transition can never leave one of them
	*        stale regardless of which code path (boot, or a downlink caught
	*        by RefreshModeAndConfig from any of RunScheduledAction/
	*        RunMotionStatusAction/RunCalibrationAction) is the one that
	*        detects it. Exposed (rather than static) so main() can call it
	*        once for the initial sync at boot, matching what
	*        RefreshModeAndConfig does on every later mode change.
	*/
void SyncModeConfiguration(AppMode_t mode);

/**
	* @brief Applies every runtime (non-mode) side effect of a config change,
	*        once the caller has already confirmed one happened via
	*        RefreshModeAndConfig: re-arms the wall-clock-aligned schedule
	*        timer with the new sleep_time_minutes/schedule_offset_minutes,
	*        and reprograms the BMA400's wake_thresh/no_motion_duration_s
	*        registers (restoring whichever GEN1/GEN2 enable state the mode
	*        calls for, since reprogramming unconditionally re-enables both
	*        as a side effect). Must be called from every
	*        place that detects a config change via RefreshModeAndConfig
	*        (RunScheduledAction, RunMotionStatusAction, RunCalibrationAction)
	*        - a downlink can land in the RX window of any of their uplinks,
	*        not just RunScheduledAction's.
	*/
static void ApplySyncedConfig(AppMode_t mode, struct AppConfig_s config);

/**
	* @brief Re-reads device mode/config against the given previous values -
	*        a downlink (e.g. SET_CONFIG_COMMMAND/SET_DEVICE_MODE_COMMAND) may
	*        have landed in the RX window of whatever uplink was just sent,
	*        regardless of which of RunScheduledAction/RunMotionStatusAction/
	*        RunRfTestLoop triggered it. Re-syncs mode configuration on a
	*        mode change and reports the new mode/config out by reference.
	* @param prev_mode    Mode captured before the uplink(s) that may have
	*                     opened an RX window.
	* @param prev_config  Config captured at the same time.
	* @param new_mode     Output - the freshly re-read mode.
	* @param new_config   Output - the freshly re-read config.
	* @return true if the mode or config actually changed.
	*/
static bool RefreshModeAndConfig(AppMode_t prev_mode, struct AppConfig_s prev_config,
                                  AppMode_t *new_mode, struct AppConfig_s *new_config);

/**
	* @brief Sending a status payload to confirm a mode/config change itself
	*        opens another RX window - a separately-queued downlink can land
	*        there too (observed in practice: two config downlinks queued
	*        back to back, the second landing during the first's own
	*        confirmation uplink). Call right after such a confirmation send;
	*        checks once more and sends a further confirmation if something
	*        changed again. Deliberately not a full loop - one extra round
	*        covers the realistic case without risking an unbounded chain if
	*        downlinks kept arriving indefinitely.
	* @param sent_mode/sent_config  What was just confirmed in the payload
	*                               that opened the RX window being re-checked.
	* @param out_mode/out_config    Output - the final mode/config after this check.
	* @return true if anything changed on this extra check.
	*/
static bool ConfirmAgainIfChangedFurther(AppMode_t sent_mode, struct AppConfig_s sent_config,
                                          AppMode_t *out_mode, struct AppConfig_s *out_config);

/**
	* @brief Delays this cycle's work by a small, deterministic, per-device
	*        offset derived from the DevEUI (0 to DEV_EUI_OFFSET_SLOT_COUNT-1
	*        slots of DEV_EUI_OFFSET_SLOT_MS each), so devices sharing the
	*        same wall-clock-aligned schedule don't all transmit at the exact
	*        same instant. Sleeps via __WFI() rather than a busy spin. No-op
	*        (returns immediately) if the DevEUI can't be read.
	*/
static void DelayForDevEuiOffset(void);

/**
	* @brief callback when LoRaWAN application Class is changed
	* @param deviceClass new class
	*/
static void OnClassChange(DeviceClass_t deviceClass);

/**
	* @brief  LoRa store context in Non Volatile Memory
	*/
static void StoreContext(void);

/**
	* @brief  stop current LoRa execution to switch into non default Activation mode
	*/
static void StopJoin(void);

/**
	* @brief  Join switch timer callback function
	* @param  context ptr of Join switch context
	*/
static void OnStopJoinTimerEvent(void *context);

/**
	* @brief  Notifies the upper layer that the NVM context has changed
	* @param  state Indicates if we are storing (true) or restoring (false) the NVM context
	*/
static void OnNvmDataChange(LmHandlerNvmContextStates_t state);

/**
	* @brief  Store the NVM Data context to the Flash
	* @param  nvm ptr on nvm structure
	* @param  nvm_size number of data bytes which were stored
	*/
static void OnStoreContextRequest(void *nvm, uint32_t nvm_size);

/**
	* @brief  Restore the NVM Data context from the Flash
	* @param  nvm ptr on nvm structure
	* @param  nvm_size number of data bytes which were restored
	*/
static void OnRestoreContextRequest(void *nvm, uint32_t nvm_size);

/**
	* Will be called each time a Radio IRQ is handled by the MAC layer
	*
	*/
static void OnMacProcessNotify(void);

/**
	* @brief Change the periodicity of the uplink frames
	* @param periodicity uplink frames period in ms
	* @note Compliance test protocol callbacks
	*/
static void OnTxPeriodicityChanged(uint32_t periodicity);

/**
	* @brief Change the confirmation control of the uplink frames
	* @param isTxConfirmed Indicates if the uplink requires an acknowledgement
	* @note Compliance test protocol callbacks
	*/
static void OnTxFrameCtrlChanged(LmHandlerMsgTypes_t isTxConfirmed);

/**
	* @brief Change the periodicity of the ping slot frames
	* @param pingSlotPeriodicity ping slot frames period in ms
	* @note Compliance test protocol callbacks
	*/
static void OnPingSlotPeriodicityChanged(uint8_t pingSlotPeriodicity);

/**
	* @brief Will be called to reset the system
	* @note Compliance test protocol callbacks
	*/
static void OnSystemReset(void);


/* Private variables ---------------------------------------------------------*/
/**
	* @brief LoRaWAN default activation type
	*/
static ActivationType_t ActivationType = LORAWAN_DEFAULT_ACTIVATION_TYPE;

/**
	* @brief LoRaWAN force rejoin even if the NVM context is restored
	*/
static bool ForceRejoin = LORAWAN_FORCE_REJOIN_AT_BOOT;

/**
	* @brief LoRaWAN handler Callbacks
	*/
static LmHandlerCallbacks_t LmHandlerCallbacks =
{
	.GetBatteryLevel =              GetBatteryLevel,
	.GetTemperature =               GetTemperatureLevel,
	.GetUniqueId =                  GetUniqueId,
	.GetDevAddr =                   GetDevAddr,
	.OnRestoreContextRequest =      OnRestoreContextRequest,
	.OnStoreContextRequest =        OnStoreContextRequest,
	.OnMacProcess =                 OnMacProcessNotify,
	.OnNvmDataChange =              OnNvmDataChange,
	.OnJoinRequest =                OnJoinRequest,
	.OnTxData =                     OnTxData,
	.OnRxData =                     OnRxData,
	.OnBeaconStatusChange =         OnBeaconStatusChange,
	.OnSysTimeUpdate =              OnSysTimeUpdate,
	.OnClassChange =                OnClassChange,
	.OnTxPeriodicityChanged =       OnTxPeriodicityChanged,
	.OnTxFrameCtrlChanged =         OnTxFrameCtrlChanged,
	.OnPingSlotPeriodicityChanged = OnPingSlotPeriodicityChanged,
	.OnSystemReset =                OnSystemReset,
};

/**
	* @brief LoRaWAN handler parameters
	*/
static LmHandlerParams_t LmHandlerParams =
{
	.ActiveRegion =             ACTIVE_REGION,
	.DefaultClass =             LORAWAN_DEFAULT_CLASS,
	.AdrEnable =                LORAWAN_ADR_STATE,
	.IsTxConfirmed =            LORAWAN_DEFAULT_CONFIRMED_MSG_STATE,
	.TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
	.TxPower =                  LORAWAN_DEFAULT_TX_POWER,
	.PingSlotPeriodicity =      LORAWAN_DEFAULT_PING_SLOT_PERIODICITY,
	.RxBCTimeout =              LORAWAN_DEFAULT_CLASS_B_C_RESP_TIMEOUT
};

/**
	* @brief Type of Event to generate application Tx
	*/
static TxEventType_t EventType = TX_ON_EVENT;

/**
	* @brief Timer to handle the application Tx
	*/
static UTIL_TIMER_Object_t TxTimer;

/**
	* @brief Tx Timer period
	*/
static UTIL_TIMER_Time_t TxPeriodicity = APP_TX_DUTYCYCLE;

/**
	* @brief Join Timer period
	*/
static UTIL_TIMER_Object_t StopJoinTimer;

/**
	* @brief Timer driving the wall-clock-aligned scheduled action. Separate
	*        from TxTimer, which OnTxPeriodicityChanged (a compliance-test
	*        protocol callback) also manipulates independently of us.
	*/
static UTIL_TIMER_Object_t ScheduledActionTimer;

/**
	* @brief Timer driving RunCalibrationAction() at a fixed short interval
	*        while in APP_MODE_CALIBRATION. Separate from ScheduledActionTimer,
	*        which is wall-clock-aligned and minute-granularity - too coarse
	*        for calibration's purpose of visualizing live accel data.
	*/
static UTIL_TIMER_Object_t CalibrationTimer;

/**
	* @brief True while CalibrationTimer has been started, so
	*        SyncCalibrationTimerEnable can tell whether it needs
	*        creating/starting vs. is already running.
	*/
static bool CalibrationTimerRunning = false;

/**
	* @brief Periodic (every IWDG_CHECK_INTERVAL_MS) checker that conditionally
	*        refreshes hiwdg - see OnIwdgCheckTimerEvent. Created once and left
	*        running forever (unlike every other timer here, never stopped).
	*/
static UTIL_TIMER_Object_t IwdgCheckTimer;

/**
	* @brief SysTime seconds at the last IwdgMarkProgress() call.
	*/
static uint32_t s_iwdg_last_progress_s;

/**
	* @brief How many seconds of silence since s_iwdg_last_progress_s is still
	*        considered normal, set by IwdgMarkProgress() to match whatever
	*        cadence governs the mode/config active at the time.
	*/
static uint32_t s_iwdg_stale_margin_s;

/**
	* @brief Configured period (minutes) for the scheduled action.
	*/
static uint16_t ScheduledActionPeriodMinutes = 0;

/**
	* @brief Configured wall-clock alignment offset (minutes past midnight
	*        UTC) for the scheduled action.
	*/
static uint16_t ScheduledActionOffsetMinutes = 0;

/**
	* @brief True once ScheduledActionTimer has fired its first (aligned)
	*        one-shot and switched over to a plain recurring period.
	*/
static bool ScheduledActionTimerIsPeriodic = false;

/**
	* @brief True until the first GEN1 (no-motion) assertion after boot has
	*        been consumed. The BMA400 is soft-reset in app_accel_init(), so
	*        its duration timer starts fresh at boot - if the device happens
	*        to be sitting still while being flashed/tested (the common
	*        case), GEN1 fires ~no_motion_duration_s after every reboot even
	*        though nothing meaningful actually changed. Lets
	*        RunMotionStatusAction suppress just that first notification
	*        without ignoring genuine no-motion events afterward.
	*/
static bool s_first_no_motion_pending = true;

/**
	* @brief True while APP_MODE_BEACON's GEN1 (no-motion) interrupt is
	*        disarmed because a stop has already been reported - re-armed once
	*        GEN2 (activity) reports movement again, so the next stop can be
	*        caught too.
	*/
static bool s_beacon_is_still = false;

/**
	* @brief Set just before a motion-triggered beacon ping's (activity or
	*        stop) SendTxData() call to skip the per-DevEUI stagger delay (see
	*        DelayForDevEuiOffset) - those pings need to go out immediately,
	*        not wait out an anti-collision slot. The periodic baseline beacon
	*        ping keeps the stagger like every other payload type. Consumed
	*        (and reset) inside SendTxData.
	*/
static bool s_skip_tx_stagger = false;

/* USER CODE BEGIN PV */
/**
	* @brief User application buffer
	*/
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];

/**
	* @brief User application data structure
	*/
static LmHandlerAppData_t AppData = { 0, 0, AppDataBuffer };

/* USER CODE END PV */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

void LoRaWAN_Init(void)
{
	/* USER CODE BEGIN LoRaWAN_Init_LV */
	uint32_t feature_version = 0UL;
	/* USER CODE END LoRaWAN_Init_LV */

	/* USER CODE BEGIN LoRaWAN_Init_1 */

	/* Get LoRaWAN APP version*/
	APP_LOG(TS_OFF, VLEVEL_M, "APPLICATION_VERSION: V%X.%X.%X\r\n",
					(uint8_t)(APP_VERSION_MAIN),
					(uint8_t)(APP_VERSION_SUB1),
					(uint8_t)(APP_VERSION_SUB2));

	/* Get MW LoRaWAN info */
	APP_LOG(TS_OFF, VLEVEL_M, "MW_LORAWAN_VERSION:  V%X.%X.%X\r\n",
					(uint8_t)(LORAWAN_VERSION_MAIN),
					(uint8_t)(LORAWAN_VERSION_SUB1),
					(uint8_t)(LORAWAN_VERSION_SUB2));

	/* Get MW SubGhz_Phy info */
	APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:    V%X.%X.%X\r\n",
					(uint8_t)(SUBGHZ_PHY_VERSION_MAIN),
					(uint8_t)(SUBGHZ_PHY_VERSION_SUB1),
					(uint8_t)(SUBGHZ_PHY_VERSION_SUB2));

	/* Get LoRaWAN Link Layer info */
	LmHandlerGetVersion(LORAMAC_HANDLER_L2_VERSION, &feature_version);
	APP_LOG(TS_OFF, VLEVEL_M, "L2_SPEC_VERSION:     V%X.%X.%X\r\n",
					(uint8_t)(feature_version >> 24),
					(uint8_t)(feature_version >> 16),
					(uint8_t)(feature_version >> 8));

	/* Get LoRaWAN Regional Parameters info */
	LmHandlerGetVersion(LORAMAC_HANDLER_REGION_VERSION, &feature_version);
	APP_LOG(TS_OFF, VLEVEL_M, "RP_SPEC_VERSION:     V%X-%X.%X.%X\r\n",
					(uint8_t)(feature_version >> 24),
					(uint8_t)(feature_version >> 16),
					(uint8_t)(feature_version >> 8),
					(uint8_t)(feature_version));

	if (FLASH_IF_Init(NULL) != FLASH_IF_OK)
	{
		Error_Handler(ERROR_FLASH_INIT);
	}

	// Diagnostic dump of the RTC backup-domain state. g_boot_bdcr is captured as the very
	// first instruction of main(), before any init runs; RTCSEL=1 means LSE (expected/correct),
	// RTCSEL=0 means the backup domain was ALREADY lost before this boot's software touched it
	// (points to shutdown/wake being electrical). g_rtc_inits/DR0/DR1 are captured later, inside
	// MX_RTC_Init() - if RTCSEL above reads correctly but these still read 0, the reset is being
	// triggered by this boot's own HAL_RCCEx_PeriphCLKConfig() RTC clock-source mismatch path.
	APP_LOG(TS_OFF, VLEVEL_M, "Boot RCC_BDCR: RTCSEL=%d RTCEN=%d LSEON=%d LSERDY=%d\r\n",
			(int)((g_boot_bdcr >> 8) & 0x3), (int)((g_boot_bdcr >> 15) & 0x1),
			(int)(g_boot_bdcr & 0x1), (int)((g_boot_bdcr >> 1) & 0x1));
	APP_LOG(TS_OFF, VLEVEL_M, "RTC backup domain: INITS=%d DR0=%d DR1=%d\r\n",
			(int)g_rtc_inits, (int)g_rtc_bkup_dr0, (int)g_rtc_bkup_dr1);
	// Live confirmation, taken right after MX_RTC_Init()/HAL_RTC_MspInit()
	// completed this boot (as opposed to g_boot_bdcr above, which is the
	// pre-init snapshot). MspInit already halts via Error_Handler() if LSE
	// isn't ready or isn't selected, so RTCSEL/LSERDY reading correctly here
	// is a direct, live confirmation LSE is genuinely driving the RTC now.
	APP_LOG(TS_OFF, VLEVEL_M, "Post-init RCC_BDCR: RTCSEL=%d RTCEN=%d LSEON=%d LSERDY=%d\r\n",
			(int)((g_rtc_bdcr_after_init >> 8) & 0x3), (int)((g_rtc_bdcr_after_init >> 15) & 0x1),
			(int)(g_rtc_bdcr_after_init & 0x1), (int)((g_rtc_bdcr_after_init >> 1) & 0x1));

	/* USER CODE END LoRaWAN_Init_1 */

	UTIL_TIMER_Create(&StopJoinTimer, JOIN_TIME, UTIL_TIMER_ONESHOT, OnStopJoinTimerEvent, NULL);

	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LmHandlerProcess), UTIL_SEQ_RFU, LmHandlerProcess);
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LoRaStoreContextEvent), UTIL_SEQ_RFU, StoreContext);
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LoRaStopJoinEvent), UTIL_SEQ_RFU, StopJoin);
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_ScheduledAction), UTIL_SEQ_RFU, RunScheduledAction);
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_MotionStatusAction), UTIL_SEQ_RFU, RunMotionStatusAction);
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_CalibrationAction), UTIL_SEQ_RFU, RunCalibrationAction);

	/* Init Info table used by LmHandler*/
	LoraInfo_Init();

	/* Init the Lora Stack*/
	LmHandlerInit(&LmHandlerCallbacks, APP_VERSION);

	// Set RX windows RX1 and RX2.
	LmHandlerSetRx1Delay(1000);
	LmHandlerSetRx2Delay(0);

	LmHandlerConfigure(&LmHandlerParams);

	/* USER CODE BEGIN LoRaWAN_Init_2 */
	// Must run before LmHandlerJoin() below - ABP join reads whatever
	// DevEUI/AppSKey/NwkSKey are currently set, so the derived Lynkz keys
	// have to be in place before that call, not in LoRaWAN_Init_Last.
	LynkzKeyGen_Apply();
	SetSelectedChannels();
	// printf("event type: %d\r\n", EventType);
	/* USER CODE END LoRaWAN_Init_2 */

	LmHandlerJoin(ActivationType, ForceRejoin);

	if (EventType == TX_ON_TIMER)
	{
		/* send every time timer elapses */
		UTIL_TIMER_Create(&TxTimer, TxPeriodicity, UTIL_TIMER_ONESHOT, OnTxTimerEvent, NULL);
		UTIL_TIMER_Start(&TxTimer);
	}
	else
	{
		/* USER CODE BEGIN LoRaWAN_Init_3 */

		/* USER CODE END LoRaWAN_Init_3 */
	}

	/* USER CODE BEGIN LoRaWAN_Init_Last */
	// DeviceTimeReq sync happens in RunScheduledAction() (called once at boot
	// right after this returns, then on every scheduled cycle) - not here, to
	// keep a single call site for "sync at boot and periodically."
	uint32_t start = HAL_GetTick();
	while(HAL_GetTick() - start < 100) {
		// wait for 500 ms max to ensure that TxDone is received and processed in the Tx callback, to avoid stopping the timer before the callback is executed.
		// Also wait for RX window.
		// This also gives any trace message queued above time to finish draining over
		// UART/DMA before MX_USART2_UART_Init() re-inits the peripheral in main(),
		// right after this function returns.
		LmHandlerProcess();
	}
	/* USER CODE END LoRaWAN_Init_Last */
}

void LoRaWAN_DeInit(void)
{
	/* USER CODE BEGIN LoRaWAN_DeInit_1 */
	int8_t rslt = true;
	
    UTIL_TIMER_Stop(&TxTimer);
	UTIL_TIMER_Stop(&StopJoinTimer);

    rslt = LmHandlerHalt();
    printf("LmHandlerHalt rslt: %d\r\n", rslt);

	/* USER CODE BEGIN DeInit_Radio */
	HAL_RADIO_DeInit();
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WRFBUSY);

	/* USER CODE END DeInit_Radio */

	/* USER CODE END LoRaWAN_DeInit_1 */
}

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params)
{
	/* USER CODE BEGIN OnRxData_1 */
	uint8_t RxPort = 0;

	if (params != NULL)
	{
		printf("RX: slot=%d rssi=%d snr=%d\r\n", params->RxSlot, params->Rssi, params->Snr);

	if (params->IsMcpsIndication)
	{
		if (appData != NULL)
		{
			RxPort = appData->Port;
			if (appData->Buffer != NULL){
				app_comm_lora_process(appData->Port, appData->Buffer, appData->BufferSize);
			}
		}
	}
	if (params->RxSlot < RX_SLOT_NONE)
	{
		APP_LOG(TS_OFF, VLEVEL_H, "###### D/L FRAME:%04d | PORT:%d | DR:%d | SLOT:%s | RSSI:%d | SNR:%d\r\n",
				params->DownlinkCounter, RxPort, params->Datarate, slotStrings[params->RxSlot],
				params->Rssi, params->Snr);
	}
	}
	/* USER CODE END OnRxData_1 */
}

/**
	* @brief Lowest data rate, starting at LORAWAN_DEFAULT_DATA_RATE (best range)
	*        and going no higher than LORAWAN_MAX_UPLINK_DATA_RATE, whose
	*        regional max application payload (N) fits size bytes. Uses the
	*        same RegionGetPhyParam(PHY_MAX_PAYLOAD) table the MAC validates
	*        against. Pending MAC commands (FOpts) are not counted here - if
	*        they don't fit alongside, SendTxData's existing flush-and-retry
	*        handles it.
	*/
static int8_t DatarateForPayload(uint8_t size)
{
	GetPhyParams_t getPhy = { 0 };
	getPhy.Attribute = PHY_MAX_PAYLOAD;
	getPhy.UplinkDwellTime = 0; // no uplink dwell-time limit in US915

	for (int8_t dr = LORAWAN_DEFAULT_DATA_RATE; dr <= LORAWAN_MAX_UPLINK_DATA_RATE; dr++) {
		getPhy.Datarate = dr;
		if (RegionGetPhyParam(ACTIVE_REGION, &getPhy).Value >= size) {
			return dr;
		}
	}
	return LORAWAN_MAX_UPLINK_DATA_RATE;
}

void SendTxData(void)
{
    /* USER CODE BEGIN SendTxData_1 */
    LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;
    UTIL_TIMER_Time_t nextTxIn = 0;

	// Prepend the Lynkz device-type byte to every uplink, regardless of which
	// Send*Payload built it - centralized here rather than in each builder so
	// nothing can forget it as new payload types get added. Done once, before
	// any retry below - re-running this on a retry would double-prepend.
	memmove(&AppData.Buffer[1], &AppData.Buffer[0], AppData.BufferSize);
	AppData.Buffer[0] = LYNKZ_DEVICE_TYPE;
	AppData.BufferSize += 1;

	// Staggers every radio send (not just the first per cycle), so a
	// status/confirmation payload sent later in the same cycle - after
	// processing a downlink, for example - doesn't re-collide with other
	// devices' sends the way an unstaggered follow-up would.
	// Skipped for beacon payloads (s_skip_tx_stagger) - those need to go out
	// immediately rather than wait out an anti-collision slot.
	if (!s_skip_tx_stagger) {
		DelayForDevEuiOffset();
	}
	s_skip_tx_stagger = false;
	LmHandlerSetTxPower(LORAWAN_DEFAULT_TX_POWER);
	int8_t tx_dr = DatarateForPayload(AppData.BufferSize);
	if (tx_dr != LORAWAN_DEFAULT_DATA_RATE) {
		APP_LOG(TS_OFF, VLEVEL_M, "Payload %d B exceeds DR%d limit, sending at DR%d\r\n",
				(int)AppData.BufferSize, (int)LORAWAN_DEFAULT_DATA_RATE, (int)tx_dr);
	}
	LmHandlerSetTxDatarate(tx_dr);

    // Up to 3 attempts, covering two independent reasons a naive single try
    // could silently lose this payload:
    //  - LmHandler busy (e.g. a motion/mount event landing mid multi-NbTrans
    //    retransmission, or two events back-to-back) - wait it out (bounded)
    //    and retry, instead of dropping on a single busy check.
    //  - LmHandlerSend() returns LORAMAC_HANDLER_PAYLOAD_LENGTH_RESTRICTED
    //    when a pending MAC command (e.g. a DeviceTimeReq queued the same
    //    cycle) doesn't leave enough room for our payload at the current DR -
    //    it still transmits, but an *empty* frame that only flushes the MAC
    //    command, silently dropping our actual payload. Retrying afterwards
    //    (once that flushed frame's own busy cycle clears) succeeds, since
    //    the MAC command is no longer pending.
    for (uint8_t attempt = 0; attempt < 3; attempt++){
        if (LmHandlerIsBusy()){
            APP_LOG(TS_ON, VLEVEL_L, "LmHandler busy, waiting to retry TX\r\n");
            uint32_t waitStart = HAL_GetTick();
            while (LmHandlerIsBusy() && (HAL_GetTick() - waitStart < LMHANDLER_BUSY_MAX_WAIT)){
                LmHandlerProcess();
            }
            if (LmHandlerIsBusy()){
                APP_LOG(TS_ON, VLEVEL_L, "LmHandler still busy after waiting, TX skipped\r\n");
                break; // still stuck busy after waiting it out - give up
            }
        }

        status = LmHandlerSend(&AppData, LmHandlerParams.IsTxConfirmed, false);
        if (LORAMAC_HANDLER_SUCCESS == status){
            APP_LOG(TS_ON, VLEVEL_L, "MESSAGE SENT\r\n");
            break;
        } else if (LORAMAC_HANDLER_DUTYCYCLE_RESTRICTED == status){
            nextTxIn = LmHandlerGetDutyCycleWaitTime();
            if (nextTxIn > 0){
                APP_LOG(TS_ON, VLEVEL_L, "Next Tx in  : ~%d second(s)\r\n", (nextTxIn / 1000));
            }
            break;
        } else if (LORAMAC_HANDLER_PAYLOAD_LENGTH_RESTRICTED == status){
            APP_LOG(TS_ON, VLEVEL_L, "Payload didn't fit alongside a pending MAC command - flushed it, retrying\r\n");
            continue;
        } else {
            APP_LOG(TS_ON, VLEVEL_L, "LmHandlerSend failed: %d\r\n", status);
            break;
        }
    }

	uint32_t start = HAL_GetTick();
		// Wait for TxDone to be received and processed in the Tx callback, to avoid stopping the timer before the
		// callback is executed. Also wait for the RX window(s).
		// Past DOWNLINK_WAIT_INTERVAL, keep draining as long as LmHandler is still busy (e.g. MIB_CHANNELS_NB_TRANS
		// > 1 makes it run multiple TX/RX cycles back-to-back) - otherwise the next SendTxData() call would see
		// LmHandlerIsBusy() still true and get silently skipped.
		// LMHANDLER_BUSY_MAX_WAIT bounds this in case LmHandler ever gets stuck busy.
	while ((HAL_GetTick() - start < DOWNLINK_WAIT_INTERVAL
			|| LmHandlerIsBusy())
			&& (HAL_GetTick() - start < LMHANDLER_BUSY_MAX_WAIT)) {
		LmHandlerProcess();
	}
    /* USER CODE END SendTxData_1 */

}

static void OnTxTimerEvent(void *context)
{
	/* USER CODE BEGIN OnTxTimerEvent_1 */
	UNUSED(context);
	/* USER CODE END OnTxTimerEvent_1 */

	/*Wait for next tx slot*/
	UTIL_TIMER_Start(&TxTimer);
	/* USER CODE BEGIN OnTxTimerEvent_2 */

	/* USER CODE END OnTxTimerEvent_2 */
}

/* USER CODE BEGIN PrFD_LedEvents */

/* USER CODE END PrFD_LedEvents */

static void OnTxData(LmHandlerTxParams_t *params)
{
	/* USER CODE BEGIN OnTxData_1 */
	if ((params != NULL))
	{
	/* Process Tx event only if its a mcps response to prevent some internal events (mlme) */
	if (params->IsMcpsConfirm != 0)
	{

		APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Confirm =============\r\n");
		APP_LOG(TS_OFF, VLEVEL_H, "###### U/L FRAME:%04d | PORT:%d | DR:%d | PWR:%d", params->UplinkCounter,
				params->AppData.Port, params->Datarate, params->TxPower);

		APP_LOG(TS_OFF, VLEVEL_H, " | MSG TYPE:");
		if (params->MsgType == LORAMAC_HANDLER_CONFIRMED_MSG)
		{
		APP_LOG(TS_OFF, VLEVEL_H, "CONFIRMED [%s]\r\n", (params->AckReceived != 0) ? "ACK" : "NACK");
		}
		else
		{
		APP_LOG(TS_OFF, VLEVEL_H, "UNCONFIRMED\r\n");
		}
	}
	}
	/* USER CODE END OnTxData_1 */
}

static void OnJoinRequest(LmHandlerJoinParams_t *joinParams)
{
	/* USER CODE BEGIN OnJoinRequest_1 */
	if (joinParams != NULL)
	{
	if (joinParams->Status == LORAMAC_HANDLER_SUCCESS)
	{

		APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOINED = ");
		if (joinParams->Mode == ACTIVATION_TYPE_ABP)
		{
		APP_LOG(TS_OFF, VLEVEL_M, "ABP ======================\r\n");
		}
		else
		{
		APP_LOG(TS_OFF, VLEVEL_M, "OTAA =====================\r\n");
		}
	}
	else
	{
		APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOIN FAILED\r\n");
	}

	APP_LOG(TS_OFF, VLEVEL_H, "###### U/L FRAME:JOIN | DR:%d | PWR:%d\r\n", joinParams->Datarate, joinParams->TxPower);
	}
	/* USER CODE END OnJoinRequest_1 */
}

static void OnBeaconStatusChange(LmHandlerBeaconParams_t *params)
{
	/* USER CODE BEGIN OnBeaconStatusChange_1 */
	if (params != NULL)
	{
	switch (params->State)
	{
		default:
		case LORAMAC_HANDLER_BEACON_LOST:
		{
		APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### BEACON LOST\r\n");
		break;
		}
		case LORAMAC_HANDLER_BEACON_RX:
		{
		APP_LOG(TS_OFF, VLEVEL_M,
				"\r\n###### BEACON RECEIVED | DR:%d | RSSI:%d | SNR:%d | FQ:%d | TIME:%d | DESC:%d | "
				"INFO:02X%02X%02X %02X%02X%02X\r\n",
				params->Info.Datarate, params->Info.Rssi, params->Info.Snr, params->Info.Frequency,
				params->Info.Time.Seconds, params->Info.GwSpecific.InfoDesc,
				params->Info.GwSpecific.Info[0], params->Info.GwSpecific.Info[1],
				params->Info.GwSpecific.Info[2], params->Info.GwSpecific.Info[3],
				params->Info.GwSpecific.Info[4], params->Info.GwSpecific.Info[5]);
		break;
		}
		case LORAMAC_HANDLER_BEACON_NRX:
		{
		APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### BEACON NOT RECEIVED\r\n");
		break;
		}
	}
	}
	/* USER CODE END OnBeaconStatusChange_1 */
}

static void OnSysTimeUpdate(void)
{
	/* USER CODE BEGIN OnSysTimeUpdate_1 */
	SysTime_t curTime = SysTimeGet();
	curTime.Seconds -= GPS_UTC_LEAP_SECONDS;
	SysTimeSet(curTime);

	app_set_last_device_time_sync(curTime.Seconds);
	LogSysTime("DeviceTimeAns: synced");

	/* Re-align the wake schedule on every successful sync, not just the
	 * first. There is no RTC hardware calibration in this codebase - the
	 * periodic wake timer and SysTimeGet() are both driven by the same
	 * uncalibrated calendar, so the crystal's real ppm error is invisible
	 * internally (the alarm and the clock agree with each other, since
	 * they're the same clock) and only becomes visible against the
	 * network's true time at each DeviceTimeAns. Without re-arming here,
	 * that error would keep accumulating in the wake schedule's *phase* for
	 * the entire session - the absolute-time correction above fixes what
	 * time we think it is, but does nothing for when a wake actually fires
	 * physically, unless the schedule is re-aligned at the same cadence.
	 * Guard against period==0: ScheduleAction() hasn't run yet the very
	 * first time a sync can land (mid heartbeat, before main() arms the
	 * schedule for the first time). */
	if (ScheduledActionPeriodMinutes != 0)
	{
		ScheduleAction(ScheduledActionPeriodMinutes, ScheduledActionOffsetMinutes);
	}
	/* USER CODE END OnSysTimeUpdate_1 */
}

void LogSysTime(const char *label)
{
	SysTime_t curTime = SysTimeGet();
	struct tm calTime;

	SysTimeLocalTime(curTime.Seconds, &calTime);
	APP_LOG(TS_OFF, VLEVEL_M, "%s: %04d-%02d-%02d %02d:%02d:%02d.%03d UTC\r\n",
			label, calTime.tm_year + 1900, calTime.tm_mon + 1, calTime.tm_mday,
			calTime.tm_hour, calTime.tm_min, calTime.tm_sec, abs(curTime.SubSeconds));
}

static bool SyncDeviceTimeIfDue(void)
{
	uint32_t last_sync = app_get_last_device_time_sync();

	/* A device whose own measurement schedule is already at least as
	 * frequent as DEVICE_TIME_SYNC_PERIOD_MINUTES just resyncs on that fixed
	 * period. One whose schedule is slower (sleep_time_minutes greater than
	 * this period) would otherwise go longer than intended between absolute-
	 * time corrections, so it resyncs at every scheduled wake instead -
	 * period_s=0 makes the "due" check below always pass. */
	struct AppConfig_s cfg = app_get_device_config();
	uint32_t period_s = (cfg.sleep_time_minutes > DEVICE_TIME_SYNC_PERIOD_MINUTES)
			? 0UL
			: (uint32_t)DEVICE_TIME_SYNC_PERIOD_MINUTES * 60UL;

	/* RTC backup domain didn't survive this reboot (SysTime reads back near
	 * the Unix epoch). Seed it from the last known-good timestamp so logs
	 * see a plausible time immediately, and force a resync this boot
	 * regardless of the configured period, since the RTC can't be trusted
	 * to have kept accurate time across this reset. */
	if (SysTimeGet().Seconds < SYSTIME_SANITY_FLOOR_SECONDS)
	{
		if (last_sync != 0)
		{
			SysTime_t seeded = { .Seconds = last_sync, .SubSeconds = 0 };
			SysTimeSet(seeded);
			LogSysTime("SysTime seeded from last known sync");
		}
		last_sync = 0;
	}

	if (last_sync != 0 && (SysTimeGet().Seconds - last_sync) < period_s)
	{
		return false;
	}

	if (LmHandlerJoinStatus() != LORAMAC_HANDLER_SET)
	{
		return false;
	}

	if (LmHandlerDeviceTimeReq() == LORAMAC_HANDLER_SUCCESS)
	{
		if (period_s == 0UL)
		{
			APP_LOG(TS_OFF, VLEVEL_M, "DeviceTimeReq queued (resync: every wake)\r\n");
		}
		else
		{
			APP_LOG(TS_OFF, VLEVEL_M, "DeviceTimeReq queued (resync period: %d min)\r\n", (int)(period_s / 60UL));
		}
		return true;
	}

	return false;
}

static uint32_t MsUntilNextAlignedWake(uint16_t period_minutes, uint16_t offset_minutes)
{
	uint32_t period_s = (uint32_t)period_minutes * 60UL;
	uint32_t offset_s = (uint32_t)offset_minutes * 60UL;
	SysTime_t now = SysTimeGet();
	uint32_t now_s = now.Seconds;
	/* SubSeconds is milliseconds into the current second (0-999) for an
	 * absolute SysTimeGet() reading. Ignoring it (as this used to) truncates
	 * the delay computed below to the next whole second, so the very first
	 * aligned wake could land up to ~999ms late - a fixed, one-time offset
	 * that every later periodic reload then inherits. */
	uint32_t ms_into_second = (uint32_t)now.SubSeconds;

	if (now_s < SYSTIME_SANITY_FLOOR_SECONDS)
	{
		/* No usable time reference yet; fall back to a plain interval for
		 * this one cycle. The next successful sync re-aligns subsequent wakes. */
		return period_s * 1000UL;
	}

	/* Align to boundaries at offset_s, offset_s+period_s, offset_s+2*period_s,
	 * ... forever - anchored to the fixed Unix epoch (itself 00:00 UTC), not
	 * to anything device- or day-specific, so every sensor with a synced
	 * clock and the same period/offset converges on the exact same grid.
	 * E.g. period=90min/offset=0 lands on 00:00/01:30/03:00/...; offset=120
	 * shifts that to 02:00/03:30/05:00/... This scales uniformly from a few
	 * minutes up to the multi-day periods SLEEP_TIME_MAX allows (7 days) -
	 * no day-boundary special case needed. For periods that don't evenly
	 * divide a day/week the grid just continues on its fixed modular
	 * pattern, still fully deterministic and fleet-consistent. */
	uint32_t next_boundary_s;
	if (now_s < offset_s)
	{
		next_boundary_s = offset_s;
	}
	else
	{
		uint32_t elapsed_s = now_s - offset_s;
		next_boundary_s = offset_s + ((elapsed_s / period_s) + 1UL) * period_s;
	}

	return (next_boundary_s - now_s) * 1000UL - ms_into_second;
}

static void OnScheduledActionTimerEvent(void *context)
{
	UNUSED(context);

	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_ScheduledAction), CFG_SEQ_Prio_0);

	if (!ScheduledActionTimerIsPeriodic)
	{
		ScheduledActionTimerIsPeriodic = true;
		UTIL_TIMER_Stop(&ScheduledActionTimer);
		UTIL_TIMER_SetReloadMode(&ScheduledActionTimer, UTIL_TIMER_PERIODIC);
		UTIL_TIMER_SetPeriod(&ScheduledActionTimer, (uint32_t)ScheduledActionPeriodMinutes * 60000UL);
		UTIL_TIMER_Start(&ScheduledActionTimer);
	}
}

void ScheduleAction(uint16_t period_minutes, uint16_t offset_minutes)
{
	ScheduledActionPeriodMinutes = period_minutes;
	ScheduledActionOffsetMinutes = offset_minutes;
	ScheduledActionTimerIsPeriodic = false;

	/* Always fully re-arm (not just adjust an already-periodic timer's
	 * period) so a period OR offset change re-aligns to the new grid
	 * immediately, rather than only taking effect after the current
	 * (now-stale) period elapses. Stop() is a harmless no-op the first time. */
	UTIL_TIMER_Stop(&ScheduledActionTimer);
	UTIL_TIMER_Create(&ScheduledActionTimer, MsUntilNextAlignedWake(period_minutes, offset_minutes),
			UTIL_TIMER_ONESHOT, OnScheduledActionTimerEvent, NULL);
	UTIL_TIMER_Start(&ScheduledActionTimer);
}

static void OnIwdgCheckTimerEvent(void *context)
{
	UNUSED(context);

	uint32_t now_s = SysTimeGet().Seconds;
	if ((now_s - s_iwdg_last_progress_s) < s_iwdg_stale_margin_s) {
		HAL_IWDG_Refresh(&hiwdg);
	}
	// else: no evidence of a completed cycle in far longer than the current
	// mode/config's own cadence allows - skip the refresh and let hiwdg's
	// hardware timeout reset the device.
}

static void IwdgMarkProgress(AppMode_t mode, struct AppConfig_s config)
{
	s_iwdg_last_progress_s = SysTimeGet().Seconds;

	if (mode == APP_MODE_CALIBRATION || mode == APP_MODE_RF_TEST) {
		s_iwdg_stale_margin_s = (uint32_t)IWDG_PROGRESS_FIXED_MARGIN_MINUTES * 60UL;
	} else {
		uint16_t base_minutes = (config.sleep_time_minutes > config.idle_sleep_time_minutes)
				? config.sleep_time_minutes : config.idle_sleep_time_minutes;
		uint32_t margin_minutes = (uint32_t)base_minutes * IWDG_PROGRESS_MARGIN_MULTIPLIER;
		if (margin_minutes < IWDG_PROGRESS_MIN_MARGIN_MINUTES) {
			margin_minutes = IWDG_PROGRESS_MIN_MARGIN_MINUTES;
		}
		s_iwdg_stale_margin_s = margin_minutes * 60UL;
	}
}

/**
	* @brief Starts IWDG and the IwdgCheckTimer that feeds it conditionally -
	*        see IwdgMarkProgress/OnIwdgCheckTimerEvent. Called once from
	*        main() after MX_LoRaWAN_Init() (needs the RTC/UTIL_TIMER already
	*        initialised), before anything else that could legitimately take
	*        a while (BMA400 init, LoRaWAN join) - IWDG_PROGRESS_BOOT_MARGIN_MINUTES
	*        covers that gap.
	*/
void IwdgMonitor_Start(void)
{
	MX_IWDG_Init();

	s_iwdg_last_progress_s = SysTimeGet().Seconds;
	s_iwdg_stale_margin_s = (uint32_t)IWDG_PROGRESS_BOOT_MARGIN_MINUTES * 60UL;

	UTIL_TIMER_Create(&IwdgCheckTimer, IWDG_CHECK_INTERVAL_MS, UTIL_TIMER_PERIODIC, OnIwdgCheckTimerEvent, NULL);
	UTIL_TIMER_Start(&IwdgCheckTimer);
}

static void OnClassChange(DeviceClass_t deviceClass)
{
	/* USER CODE BEGIN OnClassChange_1 */
	APP_LOG(TS_OFF, VLEVEL_M, "Switch to Class %c done\r\n", "ABC"[deviceClass]);
	/* USER CODE END OnClassChange_1 */
}

static void OnMacProcessNotify(void)
{
	/* USER CODE BEGIN OnMacProcessNotify_1 */

	/* USER CODE END OnMacProcessNotify_1 */
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LmHandlerProcess), CFG_SEQ_Prio_0);

	/* USER CODE BEGIN OnMacProcessNotify_2 */

	/* USER CODE END OnMacProcessNotify_2 */
}

static void OnTxPeriodicityChanged(uint32_t periodicity)
{
	/* USER CODE BEGIN OnTxPeriodicityChanged_1 */

	/* USER CODE END OnTxPeriodicityChanged_1 */
	TxPeriodicity = periodicity;

	if (TxPeriodicity == 0)
	{
	/* Revert to application default periodicity */
	TxPeriodicity = APP_TX_DUTYCYCLE;
	}

	/* Update timer periodicity */
	UTIL_TIMER_Stop(&TxTimer);
	UTIL_TIMER_SetPeriod(&TxTimer, TxPeriodicity);
	UTIL_TIMER_Start(&TxTimer);
	/* USER CODE BEGIN OnTxPeriodicityChanged_2 */

	/* USER CODE END OnTxPeriodicityChanged_2 */
}

static void OnTxFrameCtrlChanged(LmHandlerMsgTypes_t isTxConfirmed)
{
	/* USER CODE BEGIN OnTxFrameCtrlChanged_1 */

	/* USER CODE END OnTxFrameCtrlChanged_1 */
	LmHandlerParams.IsTxConfirmed = isTxConfirmed;
	/* USER CODE BEGIN OnTxFrameCtrlChanged_2 */

	/* USER CODE END OnTxFrameCtrlChanged_2 */
}

static void OnPingSlotPeriodicityChanged(uint8_t pingSlotPeriodicity)
{
	/* USER CODE BEGIN OnPingSlotPeriodicityChanged_1 */

	/* USER CODE END OnPingSlotPeriodicityChanged_1 */
	LmHandlerParams.PingSlotPeriodicity = pingSlotPeriodicity;
	/* USER CODE BEGIN OnPingSlotPeriodicityChanged_2 */

	/* USER CODE END OnPingSlotPeriodicityChanged_2 */
}

static void OnSystemReset(void)
{
	/* USER CODE BEGIN OnSystemReset_1 */

	/* USER CODE END OnSystemReset_1 */
	if ((LORAMAC_HANDLER_SUCCESS == LmHandlerHalt()) && (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET))
	{
	NVIC_SystemReset();
	}
	/* USER CODE BEGIN OnSystemReset_Last */

	/* USER CODE END OnSystemReset_Last */
}

static void StopJoin(void)
{
	UTIL_TIMER_Stop(&TxTimer);

	if (LORAMAC_HANDLER_SUCCESS != LmHandlerStop())
	{
	APP_LOG(TS_OFF, VLEVEL_M, "LmHandler Stop on going ...\r\n");
	}
	else
	{
	APP_LOG(TS_OFF, VLEVEL_M, "LmHandler Stopped\r\n");
	if (LORAWAN_DEFAULT_ACTIVATION_TYPE == ACTIVATION_TYPE_ABP)
	{
		ActivationType = ACTIVATION_TYPE_OTAA;
		APP_LOG(TS_OFF, VLEVEL_M, "LmHandler switch to OTAA mode\r\n");
	}
	else
	{
		ActivationType = ACTIVATION_TYPE_ABP;
		APP_LOG(TS_OFF, VLEVEL_M, "LmHandler switch to ABP mode\r\n");
	}
	LmHandlerConfigure(&LmHandlerParams);
	LmHandlerJoin(ActivationType, true);
	UTIL_TIMER_Start(&TxTimer);
	}
	UTIL_TIMER_Start(&StopJoinTimer);
	/* USER CODE BEGIN StopJoin_Last */

	/* USER CODE END StopJoin_Last */
}

static void OnStopJoinTimerEvent(void *context)
{
	/* USER CODE BEGIN OnStopJoinTimerEvent_1 */
	UNUSED(context);

	/* USER CODE END OnStopJoinTimerEvent_1 */
	if (ActivationType == LORAWAN_DEFAULT_ACTIVATION_TYPE)
	{
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaStopJoinEvent), CFG_SEQ_Prio_0);
	}
}

static void StoreContext(void)
{
	LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;

	/* USER CODE BEGIN StoreContext_1 */

	/* USER CODE END StoreContext_1 */
	status = LmHandlerNvmDataStore();

	if (status == LORAMAC_HANDLER_NVM_DATA_UP_TO_DATE)
	{
	APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA UP TO DATE\r\n");
	}
	else if (status == LORAMAC_HANDLER_ERROR)
	{
	APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA STORE FAILED\r\n");
	}
	/* USER CODE BEGIN StoreContext_Last */

	/* USER CODE END StoreContext_Last */
}

static void OnNvmDataChange(LmHandlerNvmContextStates_t state)
{
	/* USER CODE BEGIN OnNvmDataChange_1 */

	/* USER CODE END OnNvmDataChange_1 */
	if (state == LORAMAC_HANDLER_NVM_STORE)
	{
	APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA STORED\r\n");
	}
	else
	{
	APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA RESTORED\r\n");
	}
	/* USER CODE BEGIN OnNvmDataChange_Last */

	/* USER CODE END OnNvmDataChange_Last */
}

static void OnStoreContextRequest(void *nvm, uint32_t nvm_size)
{
	/* USER CODE BEGIN OnStoreContextRequest_1 */

	/* USER CODE END OnStoreContextRequest_1 */
	/* store nvm in flash */
	if (FLASH_IF_Erase(LORAWAN_NVM_BASE_ADDRESS, FLASH_PAGE_SIZE) == FLASH_IF_OK)
	{
	FLASH_IF_Write(LORAWAN_NVM_BASE_ADDRESS, (const void *)nvm, nvm_size);
	}
	/* USER CODE BEGIN OnStoreContextRequest_Last */

	/* USER CODE END OnStoreContextRequest_Last */
}

static void OnRestoreContextRequest(void *nvm, uint32_t nvm_size)
{
	/* USER CODE BEGIN OnRestoreContextRequest_1 */

	/* USER CODE END OnRestoreContextRequest_1 */
	FLASH_IF_Read(nvm, LORAWAN_NVM_BASE_ADDRESS, nvm_size);
	/* USER CODE BEGIN OnRestoreContextRequest_Last */

	/* USER CODE END OnRestoreContextRequest_Last */
}

/* USER CODE BEGIN PRIVATE FUNCTIONS */
void SendBeaconPayload(int16_t x_8mg, int16_t y_8mg, int16_t z_8mg)
{
    AppData.Port = LORAWAN_USER_BEACON_PORT;
    memcpy(&AppData.Buffer[0], &x_8mg, sizeof(x_8mg));
    memcpy(&AppData.Buffer[2], &y_8mg, sizeof(y_8mg));
    memcpy(&AppData.Buffer[4], &z_8mg, sizeof(z_8mg));
    AppData.BufferSize = sizeof(x_8mg) + sizeof(y_8mg) + sizeof(z_8mg);

    printf("Beacon payload (vibration RMS, x8mg units): X=%d Y=%d Z=%d\r\n", x_8mg, y_8mg, z_8mg);
    SendTxData();
}

void SendCalibrationPayload(int16_t x_8mg, int16_t y_8mg, int16_t z_8mg)
{
    AppData.Port = LORAWAN_USER_CALIBRATION_PORT;
    memcpy(&AppData.Buffer[0], &x_8mg, sizeof(x_8mg));
    memcpy(&AppData.Buffer[2], &y_8mg, sizeof(y_8mg));
    memcpy(&AppData.Buffer[4], &z_8mg, sizeof(z_8mg));
    AppData.BufferSize = sizeof(x_8mg) + sizeof(y_8mg) + sizeof(z_8mg);

    printf("Calibration payload (x8mg units): X=%d Y=%d Z=%d\r\n", x_8mg, y_8mg, z_8mg);
    SendTxData();
}

void SendStatusPayload(AppMode_t mode, struct AppConfig_s config)
{
    AppData.Port = LORAWAN_USER_STATUS_PORT;
    AppData.Buffer[0] = mode;
    memcpy(&AppData.Buffer[1], (uint8_t *)&config, sizeof(struct AppConfig_s));
    AppData.BufferSize = 1 + sizeof(struct AppConfig_s);

    printf("Status payload:");
    for (uint8_t i = 0; i < AppData.BufferSize; i++) {
        printf(" %02X", AppData.Buffer[i]);
    }
    printf("\r\n");
    SendTxData();
}

void SendErrorCode(uint8_t error_code){
	uint8_t i = 0;

    AppData.Port = LORAWAN_USER_ERROR_PORT;

    AppData.Buffer[i++] = (uint8_t)(error_code & 0xFF);         // Include error code in payload
    AppData.BufferSize = i;
    SendTxData();
}

void SendLoRaModTestPayload(void)
{
	RadioSetDefaultConfig();
	radio_tone();
}

void SendRfTestConfigPayload(void)
{
    radio_stop(); // Always stop an in-progress test tone before a LoRaWAN uplink.

    struct AppRfTestConfig_s rf_config = app_get_rf_test_config();
    AppData.Port = LORAWAN_USER_RF_TEST_PORT;
    memcpy(AppData.Buffer, (uint8_t *)&rf_config, sizeof(struct AppRfTestConfig_s));
    AppData.BufferSize = sizeof(struct AppRfTestConfig_s);
    printf("RF_TEST: config uplink freq=%lu Hz\r\n", rf_config.frequency_hz);
    SendTxData();
}

void RadioSetDefaultConfig(void)
{
	ModParameter_t modParam = { 0 };
	struct AppRfTestConfig_s rf = app_get_rf_test_config();

	radio_get_config(&modParam);

	modParam.modulation      = TEST_LORA;
	modParam.freq            = rf.frequency_hz;
	modParam.power           = rf.power_dbm;
	modParam.bandwidth       = rf.bandwidth;
	modParam.loraSf_datarate = rf.sf;
	modParam.codingRate      = rf.coding_rate;
	modParam.lna             = rf.lna;
	modParam.paBoost         = rf.pa_boost;
	modParam.lowDrOpt        = rf.low_dr_opt;

	radio_set_config(&modParam);

	printf("RF config: freq=%lu Hz pwr=%d dBm bw=%u sf=%u cr=%u lna=%u paBoost=%u lowDrOpt=%u\r\n",
	       rf.frequency_hz, rf.power_dbm, rf.bandwidth,
	       rf.sf, rf.coding_rate, rf.lna, rf.pa_boost, rf.low_dr_opt);
}


static void RunRfTestLoop(void)
{
	uint32_t cycle = 0;
	bool rf_test_active = true;

	while (rf_test_active) {
		struct AppRfTestConfig_s rf_cfg_before = app_get_rf_test_config();
		AppMode_t mode_before = app_get_device_mode();
		struct AppConfig_s config_before = app_get_device_config();

		printf("RF_TEST: tone on for %u ms (cycle=%lu)\r\n", RF_TEST_TONE_DURATION_MS, cycle);
		SendLoRaModTestPayload();
		LM_Delay(RF_TEST_TONE_DURATION_MS, 0);

		// Config uplink stops the tone and opens a downlink window so the
		// user can send SET_RF_TEST_CONFIG or SET_DEVICE_MODE.
		printf("RF_TEST: config uplink (cycle=%lu)\r\n", cycle);
		SendRfTestConfigPayload();
		LM_Delay(500, 0);

		if (app_rf_test_config_changed(rf_cfg_before, app_get_rf_test_config())) {
			printf("RF_TEST: config updated via downlink, sending confirmation\r\n");
			SendRfTestConfigPayload();
			LM_Delay(500, 0);
		}

		AppMode_t mode_after;
		struct AppConfig_s config_after;
		if (RefreshModeAndConfig(mode_before, config_before, &mode_after, &config_after)) {
			printf("RF_TEST: device mode/config changed via downlink, sending status confirmation\r\n");
			SendStatusPayload(mode_after, config_after);
			LM_Delay(500, 0);
		}

		IwdgMarkProgress(APP_MODE_RF_TEST, config_after);

		if (mode_after != APP_MODE_RF_TEST) {
			rf_test_active = false;
			break;
		}
		cycle++;
	}
}

static void SyncActivityInterruptEnable(AppMode_t mode)
{
	app_accel_set_activity_interrupt_enabled(mode == APP_MODE_BEACON);
}

/**
	* @brief GEN1 (no-motion) re-asserts repeatedly for as long as the device
	*        stays still (same not-edge-triggered behavior as GEN2 - see
	*        app_accel_set_no_motion_interrupt_enabled). Only meaningful while
	*        actively waiting for the *next* stillness transition, so it's
	*        disabled once a stop is reported and re-armed once activity
	*        resumes.
	*/
static void SyncNoMotionInterruptEnable(AppMode_t mode)
{
	app_accel_set_no_motion_interrupt_enabled(mode == APP_MODE_BEACON && !s_beacon_is_still);
}


static void OnCalibrationTimerEvent(void *context)
{
	UNUSED(context);
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_CalibrationAction), CFG_SEQ_Prio_0);
}

static void SyncCalibrationTimerEnable(AppMode_t mode)
{
	bool should_enable = (mode == APP_MODE_CALIBRATION);

	if (should_enable && !CalibrationTimerRunning) {
		UTIL_TIMER_Create(&CalibrationTimer, CALIBRATION_INTERVAL_MS, UTIL_TIMER_PERIODIC, OnCalibrationTimerEvent, NULL);
		UTIL_TIMER_Start(&CalibrationTimer);
		CalibrationTimerRunning = true;
	} else if (!should_enable && CalibrationTimerRunning) {
		UTIL_TIMER_Stop(&CalibrationTimer);
		CalibrationTimerRunning = false;
	}
}

void SyncModeConfiguration(AppMode_t mode)
{
	SyncActivityInterruptEnable(mode);
	SyncNoMotionInterruptEnable(mode);
	SyncCalibrationTimerEnable(mode);

	// Alive-blink burst length shows the mode: enum value + 1 pulses, so
	// STORAGE (0) still blinks once.
	gpio_alive_blink_set_count((uint8_t)mode + 1U);
}

static void ApplySyncedConfig(AppMode_t mode, struct AppConfig_s config)
{
	ScheduleAction(config.sleep_time_minutes, config.schedule_offset_minutes);

	// wake_thresh/no_motion_duration_s only take effect in the BMA400's own
	// registers if reprogrammed - app_get_device_config() reflects a
	// downlink's new value immediately, but the hardware interrupt
	// threshold/duration don't follow along on their own (otherwise only set
	// once, at boot - see main.c). Re-apply them here, then restore whichever
	// GEN1/GEN2 enable/disable state the current mode actually
	// calls for - both app_accel_configure_*() calls unconditionally
	// re-enable their interrupt as a side effect of reprogramming it, which
	// would wrongly re-arm one this session had deliberately disarmed (e.g.
	// GEN1 while a beacon is already still).
	app_accel_configure_no_motion_interrupt(config.wake_thresh, config.no_motion_duration_s);
	app_accel_configure_activity_interrupt(config.wake_thresh);
	SyncModeConfiguration(mode);
}

static bool RefreshModeAndConfig(AppMode_t prev_mode, struct AppConfig_s prev_config,
                                  AppMode_t *new_mode, struct AppConfig_s *new_config)
{
	*new_mode = app_get_device_mode();
	*new_config = app_get_device_config();

	bool mode_changed = app_mode_changed(prev_mode, *new_mode);
	if (mode_changed) {
		SyncModeConfiguration(*new_mode);
	}

	return mode_changed || app_config_changed(prev_config, *new_config);
}

static bool ConfirmAgainIfChangedFurther(AppMode_t sent_mode, struct AppConfig_s sent_config,
                                          AppMode_t *out_mode, struct AppConfig_s *out_config)
{
	bool changed = RefreshModeAndConfig(sent_mode, sent_config, out_mode, out_config);
	if (changed) {
		printf("Sending status payload (config changed again)\r\n");
		SendStatusPayload(*out_mode, *out_config);
		LM_Delay(500, 0);
	}
	return changed;
}

static void DelayForDevEuiOffset(void)
{
	uint8_t dev_eui[8] = {0};
	if (LmHandlerGetDevEUI(dev_eui) != LORAMAC_HANDLER_SUCCESS) {
		return;
	}

	// FNV-1a: every input bit influences every output bit (unlike a plain
	// XOR-fold, whose low bits - the only ones that survive "% slot count"
	// below - depend only on the low bits of each input byte). Matters here
	// because we can't assume which part of a fleet's DevEUIs actually
	// varies from device to device.
	uint32_t hash = 2166136261u;
	for (uint8_t i = 0; i < sizeof(dev_eui); i++) {
		hash ^= dev_eui[i];
		hash *= 16777619u;
	}
	uint32_t slot = hash % DEV_EUI_OFFSET_SLOT_COUNT;
	// APP_LOG's tiny formatter doesn't support %lu/%ld (established earlier
	// this session) - %d/(int) casts instead, this file's usual convention.
	APP_LOG(TS_OFF, VLEVEL_M, "DevEUI offset slot: %d/%d (%d ms)\r\n",
			(int)slot, (int)DEV_EUI_OFFSET_SLOT_COUNT,
			(int)(slot * DEV_EUI_OFFSET_SLOT_MS));

	uint32_t target_tick = HAL_GetTick() + slot * DEV_EUI_OFFSET_SLOT_MS;
	while (HAL_GetTick() < target_tick) {
		__WFI();
	}
}

void RunScheduledAction(void)
{
	AppMode_t current_mode = app_get_device_mode();
	struct AppConfig_s current_config = app_get_device_config();
	struct AppRfTestConfig_s prev_rf_config = app_get_rf_test_config();
	static bool first_run = true;

	bool sync_due = SyncDeviceTimeIfDue();

	// Boot (first_run) only syncs time and reports status below - no
	// beacon ping until the first real scheduled tick.
	if (!first_run) {
		if (current_mode == APP_MODE_BEACON) {
			// Baseline "still here" ping on the normal scheduled cadence, even
			// while stationary - RunMotionStatusAction's activity-triggered
			// ping (immediate, no rate limiting) is what makes it "faster"
			// once actually moving; without this, a beacon that never moves
			// (e.g. sitting still after falling, before being picked up)
			// would never be heard from at all.
			int16_t x_8mg = 0, y_8mg = 0, z_8mg = 0;
			app_accel_measure_vibration_short_8mg(&x_8mg, &y_8mg, &z_8mg);
			printf("Beacon: periodic ping\r\n");
			SendBeaconPayload(x_8mg, y_8mg, z_8mg);
			LM_Delay(500, 0);
		}
	}

	// Re-read mode/config in case a downlink changed them during the send above.
	AppMode_t prev_mode = current_mode;
	struct AppConfig_s prev_config = current_config;
	bool config_changed = RefreshModeAndConfig(prev_mode, prev_config, &current_mode, &current_config);

	// Status (mode+config) is occasional, not sent every cycle: on boot, when
	// a downlink actually changes mode/config, and whenever a DeviceTimeAns
	// resync landed - piggybacking on that cadence needs no new timer/state.
	// STORAGE and OPERATION have no payload of their own (see above), so
	// their "periodic message" *is* this status payload - OPERATION's is
	// MissMetal's heartbeat - sent every cycle instead of only on those
	// occasional triggers.
	bool status_periodic = (current_mode == APP_MODE_STORAGE)
	                    || (current_mode == APP_MODE_OPERATION);
	if (first_run || config_changed || sync_due || status_periodic) {
		printf("Sending status payload (%s)\r\n",
				first_run ? "boot" : config_changed ? "config changed" :
				sync_due ? "sync" : "periodic heartbeat");
		SendStatusPayload(current_mode, current_config);
		LM_Delay(500, 0);

		if (ConfirmAgainIfChangedFurther(current_mode, current_config, &current_mode, &current_config)) {
			config_changed = true;
		}
	}
	first_run = false;

	if (config_changed) {
		ApplySyncedConfig(current_mode, current_config);
	}

	if (app_rf_test_config_changed(prev_rf_config, app_get_rf_test_config())) {
		printf("RF test config changed from downlink, sending confirmation\r\n");
		SendRfTestConfigPayload();
		LM_Delay(500, 0);
	}

	// RunRfTestLoop() marks its own progress every iteration (it can block
	// here for as long as RF_TEST mode stays active) - everything else
	// completes a normal, bounded cycle right here.
	if (current_mode == APP_MODE_RF_TEST) {
		RunRfTestLoop();
	} else {
		IwdgMarkProgress(current_mode, current_config);
	}
}

void RunMotionStatusAction(void)
{
	bool no_motion = false, activity = false;
	app_accel_get_motion_status(&no_motion, &activity);

	AppMode_t mode = app_get_device_mode();
	struct AppConfig_s cfg = app_get_device_config();

	if (no_motion && mode == APP_MODE_BEACON) {
		if (s_first_no_motion_pending) {
			s_first_no_motion_pending = false;
			printf("No motion detected (first after boot, suppressing notification)\r\n");
		} else {
			int16_t x_8mg = 0, y_8mg = 0, z_8mg = 0;
			app_accel_measure_vibration_short_8mg(&x_8mg, &y_8mg, &z_8mg);

			printf("Beacon: motion stopped, sending ping\r\n");
			s_skip_tx_stagger = true;
			SendBeaconPayload(x_8mg, y_8mg, z_8mg);
			LM_Delay(500, 0);
		}

		s_beacon_is_still = true;
		// Now waiting for the return-to-motion transition - disarm GEN1 (it
		// re-asserts repeatedly while still, so it would keep spamming this
		// payload for as long as the device stays put).
		SyncModeConfiguration(mode);
	}

	if (activity && mode == APP_MODE_BEACON) {
		// Re-arm GEN1 (disarmed above once a stop was reported) so the next
		// stop can be caught too - idempotent while motion continues, since
		// GEN2 re-asserts repeatedly (not edge-triggered) and keeps calling
		// this every time.
		s_beacon_is_still = false;
		SyncModeConfiguration(mode);

		int16_t x_8mg = 0, y_8mg = 0, z_8mg = 0;
		app_accel_measure_vibration_short_8mg(&x_8mg, &y_8mg, &z_8mg);

		printf("Beacon: motion detected, sending ping\r\n");
		s_skip_tx_stagger = true;
		SendBeaconPayload(x_8mg, y_8mg, z_8mg);
		LM_Delay(500, 0);
	}

	// A downlink (e.g. SET_CONFIG_COMMMAND) may have landed in the RX window
	// opened by whichever uplink was just sent above - re-check and confirm
	// via a status payload the same way RunScheduledAction does, rather than
	// leaving it unconfirmed until the next scheduled tick notices.
	AppMode_t new_mode;
	struct AppConfig_s new_cfg;
	if (RefreshModeAndConfig(mode, cfg, &new_mode, &new_cfg)) {
		printf("Sending status payload (config changed)\r\n");
		SendStatusPayload(new_mode, new_cfg);
		LM_Delay(500, 0);
		ApplySyncedConfig(new_mode, new_cfg);
	}
	IwdgMarkProgress(new_mode, new_cfg);
}

void RunCalibrationAction(void)
{
	AppMode_t mode = app_get_device_mode();
	struct AppConfig_s cfg = app_get_device_config();
	if (mode != APP_MODE_CALIBRATION) {
		// A downlink switched mode away from CALIBRATION, but this timer fire
		// was already queued before that got a chance to stop it - resync
		// every piece of mode-dependent configuration now (not just the
		// calibration timer, or e.g. a switch to BEACON would leave GEN2
		// disarmed with nothing left to ever re-arm it) instead of sending a
		// stale calibration payload in the new mode.
		SyncModeConfiguration(mode);
		IwdgMarkProgress(mode, cfg);
		return;
	}

	int16_t x_8mg = 0, y_8mg = 0, z_8mg = 0;
	app_accel_measure_vibration_short_8mg(&x_8mg, &y_8mg, &z_8mg);

	printf("Calibration: X=%d Y=%d Z=%d (vibration RMS, x8mg units)\r\n", x_8mg, y_8mg, z_8mg);
	SendCalibrationPayload(x_8mg, y_8mg, z_8mg);

	// A downlink (e.g. switching mode) may have landed in the RX window this
	// uplink just opened - re-check and confirm via a status payload the same
	// way RunScheduledAction/RunMotionStatusAction do, rather than leaving it
	// unconfirmed (and GEN2/timer arming unsynced) until something else
	// happens to notice.
	AppMode_t new_mode;
	struct AppConfig_s new_cfg;
	if (RefreshModeAndConfig(mode, cfg, &new_mode, &new_cfg)) {
		printf("Sending status payload (config changed)\r\n");
		SendStatusPayload(new_mode, new_cfg);
		LM_Delay(500, 0);
		ApplySyncedConfig(new_mode, new_cfg);
	}
	IwdgMarkProgress(new_mode, new_cfg);
}

/* USER CODE END PRIVATE FUNCTIONS */
