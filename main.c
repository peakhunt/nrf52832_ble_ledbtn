#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "fds.h"
#include "peer_manager.h"
#include "bsp_btn_ble.h"
#include "sensorsim.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"
#include "app_scheduler.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ble_hkim_ledbtns.h"
#include "led_control.h"
#include "btn_control.h"

#define DEVICE_NAME                     "LEDButton"                             /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "KongjaEx"                              /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                300                                     /**< The advertising interval (in units of 0.625 ms. This value corresponds to 187.5 ms). */

#define APP_ADV_DURATION                18000                                   /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
#define APP_BLE_OBSERVER_PRIO           3                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)        /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)        /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                   0                                       /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)         /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                   /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                  /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                       /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                  1                                       /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                       /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                  0                                       /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS              0                                       /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                    /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                       /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                      /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define SCHED_MAX_EVENT_DATA_SIZE       APP_TIMER_SCHED_EVENT_DATA_SIZE             /**< Maximum size of scheduler events. */
#ifdef SVCALL_AS_NORMAL_FUNCTION
#define SCHED_QUEUE_SIZE                20                                          /**< Maximum number of events in the scheduler queue. More is needed in case of Serialization. */
#else
#define SCHED_QUEUE_SIZE                10                                          /**< Maximum number of events in the scheduler queue. */
#endif

NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                         /**< Context for the Queued Write module.*/
BLE_HKIM_LEDBTNS_DEF(m_ledbtns);

BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        /**< Handle of the current connection. */


#if 0
static ble_uuid_t m_adv_uuids[] =                                               /**< Universally unique service identifiers. */
{
  {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE},
};
#else
static ble_uuid_t m_adv_uuids[] =                                               /**< Universally unique service identifiers. */
{
  {BLE_UUID_HKIM_LEDBTN_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN }
};
#endif

static pm_peer_id_t      m_peer_id;                                                 /**< Device reference handle to the current bonded central. */
static pm_peer_id_t      m_whitelist_peers[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];       /**< List of peers currently in the whitelist. */
static uint32_t          m_whitelist_peer_cnt;                                      /**< Number of peers currently in the whitelist. */

static bool               m_btn_noti_enabled = false;


static void advertising_start(bool erase_bonds);


void
assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
  app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void
pm_evt_handler(pm_evt_t const * p_evt)
{
  ret_code_t err_code;

  switch (p_evt->evt_id)
  {
  case PM_EVT_BONDED_PEER_CONNECTED:
    {
      NRF_LOG_INFO("Connected to a previously bonded device.");
    } break;

  case PM_EVT_CONN_SEC_SUCCEEDED:
    {
      NRF_LOG_INFO("Connection secured: role: %d, conn_handle: 0x%x, procedure: %d.",
          ble_conn_state_role(p_evt->conn_handle),
          p_evt->conn_handle,
          p_evt->params.conn_sec_succeeded.procedure);
      m_peer_id = p_evt->peer_id;
    } break;

  case PM_EVT_CONN_SEC_FAILED:
    NRF_LOG_INFO("PM_EVT_CONN_SEC_FAILED.");
    {
      /* Often, when securing fails, it shouldn't be restarted, for security reasons.
       * Other times, it can be restarted directly.
       * Sometimes it can be restarted, but only after changing some Security Parameters.
       * Sometimes, it cannot be restarted until the link is disconnected and reconnected.
       * Sometimes it is impossible, to secure the link, or the peer device does not support it.
       * How to handle this error is highly application dependent. */
    } break;

  case PM_EVT_CONN_SEC_CONFIG_REQ:
    {
      // Reject pairing request from an already bonded peer.
      pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false};
      pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
    } break;

  case PM_EVT_STORAGE_FULL:
    NRF_LOG_INFO("PM_EVT_STORAGE_FULL.");
    {
      // Run garbage collection on the flash.
      err_code = fds_gc();
      if (err_code == FDS_ERR_NO_SPACE_IN_QUEUES)
      {
        // Retry.
      }
      else
      {
        APP_ERROR_CHECK(err_code);
      }
    } break;

  case PM_EVT_PEERS_DELETE_SUCCEEDED:
    {
      advertising_start(false);
    } break;

  case PM_EVT_PEER_DATA_UPDATE_FAILED:
    {
      // Assert.
      APP_ERROR_CHECK(p_evt->params.peer_data_update_failed.error);
    } break;

  case PM_EVT_PEER_DELETE_FAILED:
    {
      // Assert.
      APP_ERROR_CHECK(p_evt->params.peer_delete_failed.error);
    } break;

  case PM_EVT_PEERS_DELETE_FAILED:
    {
      // Assert.
      APP_ERROR_CHECK(p_evt->params.peers_delete_failed_evt.error);
    } break;

  case PM_EVT_ERROR_UNEXPECTED:
    {
      // Assert.
      APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
    } break;

  case PM_EVT_CONN_SEC_START:
  case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
    NRF_LOG_INFO("PM_EVT_PEER_DATA_UPDATE_SUCCEEDED.");
    {
      if (     p_evt->params.peer_data_update_succeeded.flash_changed
          && (p_evt->params.peer_data_update_succeeded.data_id == PM_PEER_DATA_ID_BONDING))
      {
        NRF_LOG_INFO("New Bond, add the peer to the whitelist if possible");
        NRF_LOG_INFO("\tm_whitelist_peer_cnt %d, MAX_PEERS_WLIST %d",
            m_whitelist_peer_cnt + 1,
            BLE_GAP_WHITELIST_ADDR_MAX_COUNT);
        // Note: You should check on what kind of white list policy your application should use.

        if (m_whitelist_peer_cnt < BLE_GAP_WHITELIST_ADDR_MAX_COUNT)
        {
          // Bonded to a new peer, add it to the whitelist.
          m_whitelist_peers[m_whitelist_peer_cnt++] = m_peer_id;

          // The whitelist has been modified, update it in the Peer Manager.
          err_code = pm_whitelist_set(m_whitelist_peers, m_whitelist_peer_cnt);
          APP_ERROR_CHECK(err_code);

          err_code = pm_device_identities_list_set(m_whitelist_peers, m_whitelist_peer_cnt);
          if (err_code != NRF_ERROR_NOT_SUPPORTED)
          {
            APP_ERROR_CHECK(err_code);
          }
        }
      }
    }
    break;

  case PM_EVT_PEER_DELETE_SUCCEEDED:
  case PM_EVT_LOCAL_DB_CACHE_APPLIED:
  case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
    // This can happen when the local DB has changed.
  case PM_EVT_SERVICE_CHANGED_IND_SENT:
  case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
  default:
    NRF_LOG_INFO("fucking default.");
    break;
  }
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void
timers_init(void)
{
  // Initialize timer module.
  ret_code_t err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);

  // Create timers.

  /* YOUR_JOB: Create any timers to be used by the application.
     Below is an example of how to create a timer.
     For every new timer needed, increase the value of the macro APP_TIMER_MAX_TIMERS by
     one.
     ret_code_t err_code;
     err_code = app_timer_create(&m_app_timer_id, APP_TIMER_MODE_REPEATED, timer_timeout_handler);
     APP_ERROR_CHECK(err_code); */
}

static void
gap_params_init(void)
{
  ret_code_t              err_code;
  ble_gap_conn_params_t   gap_conn_params;
  ble_gap_conn_sec_mode_t sec_mode;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

  err_code = sd_ble_gap_device_name_set(&sec_mode,
      (const uint8_t *)DEVICE_NAME,
      strlen(DEVICE_NAME));
  APP_ERROR_CHECK(err_code);

  err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_UNKNOWN);
  APP_ERROR_CHECK(err_code);

  memset(&gap_conn_params, 0, sizeof(gap_conn_params));

  gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
  gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
  gap_conn_params.slave_latency     = SLAVE_LATENCY;
  gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

  err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
  APP_ERROR_CHECK(err_code);
}

static void
gatt_init(void)
{
  ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
  APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void
nrf_qwr_error_handler(uint32_t nrf_error)
{
  APP_ERROR_HANDLER(nrf_error);
}


static void
on_ble_hkim_leds_evt(ble_hkim_ledbtns_t* p_hkim_btnleds, ble_hkim_ledbtns_evt_t* p_evt)
{
  static const char* debug_str[] =
  {
    "BLE_HKIM_LEDBTNS_EVT_LED_UPDATED",
    "BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_ENABLED",
    "BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_DISABLED",
    "BLE_HKIM_LEDBTNS_EVT_CONNECTED",
    "BLE_HKIM_LEDBTNS_EVT_DISCONNECTED",
  };

  NRF_LOG_INFO("%s event: %s", __func__, debug_str[p_evt->evt_type]);

  switch(p_evt->evt_type)
  {
  case BLE_HKIM_LEDBTNS_EVT_LED_UPDATED:
    NRF_LOG_INFO("new LED status: %02x", p_hkim_btnleds->leds_status);
    led_control_set(p_hkim_btnleds->leds_status);
    break;

  case BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_ENABLED:
    m_btn_noti_enabled = true;
    break;

  case BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_DISABLED:
    m_btn_noti_enabled = false;
    break;

  case BLE_HKIM_LEDBTNS_EVT_CONNECTED:
    break;

  case BLE_HKIM_LEDBTNS_EVT_DISCONNECTED:
    m_btn_noti_enabled = false;
    break;
  }
}

static void
services_init(void)
{
  ret_code_t         err_code;
  nrf_ble_qwr_init_t qwr_init = {0};
  ble_hkim_ledbtns_init_t   ledbtns_init = {0};

  // Initialize Queued Write Module.
  qwr_init.error_handler = nrf_qwr_error_handler;

  err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
  APP_ERROR_CHECK(err_code);

  ledbtns_init.evt_handler = on_ble_hkim_leds_evt;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ledbtns_init.hkim_leds_value_char_attr_md.cccd_write_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ledbtns_init.hkim_leds_value_char_attr_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ledbtns_init.hkim_leds_value_char_attr_md.write_perm);

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ledbtns_init.hkim_btns_value_char_attr_md.cccd_write_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ledbtns_init.hkim_btns_value_char_attr_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ledbtns_init.hkim_btns_value_char_attr_md.write_perm);

  err_code = ble_hkim_ledbtns_init(&m_ledbtns, &ledbtns_init);
  APP_ERROR_CHECK(err_code);
}

static void
on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
  ret_code_t err_code;

  NRF_LOG_INFO("on_conn_params_evt.");

  if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
  {
    NRF_LOG_INFO("BLE_CONN_PARAMS_EVT_FAILED.");
    err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
    APP_ERROR_CHECK(err_code);
  }
}

static void
conn_params_error_handler(uint32_t nrf_error)
{
  APP_ERROR_HANDLER(nrf_error);
}

static void
conn_params_init(void)
{
  ret_code_t             err_code;
  ble_conn_params_init_t cp_init;

  memset(&cp_init, 0, sizeof(cp_init));

  cp_init.p_conn_params                  = NULL;
  cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
  cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
  cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
  cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
  cp_init.disconnect_on_fail             = false;
  cp_init.evt_handler                    = on_conn_params_evt;
  cp_init.error_handler                  = conn_params_error_handler;

  err_code = ble_conn_params_init(&cp_init);
  APP_ERROR_CHECK(err_code);
}

static void
application_timers_start(void)
{
  /* YOUR_JOB: Start your timers. below is an example of how to start a timer.
     ret_code_t err_code;
     err_code = app_timer_start(m_app_timer_id, TIMER_INTERVAL, NULL);
     APP_ERROR_CHECK(err_code); */

}

static void
sleep_mode_enter(void)
{
  ret_code_t err_code;

  err_code = bsp_indication_set(BSP_INDICATE_IDLE);
  APP_ERROR_CHECK(err_code);

  // Prepare wakeup buttons.
  err_code = bsp_btn_ble_sleep_mode_prepare();
  APP_ERROR_CHECK(err_code);

  // Go to system-off mode (this function will not return; wakeup will cause a reset).
  err_code = sd_power_system_off();
  APP_ERROR_CHECK(err_code);
}

static void
on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
  ret_code_t err_code;

  switch (ble_adv_evt)
  {
  case BLE_ADV_EVT_FAST:
    NRF_LOG_INFO("Fast advertising.");
    err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_ADV_EVT_IDLE:
    NRF_LOG_INFO("BLE_ADV_EVT_IDLE.");
    sleep_mode_enter();
    break;

  case BLE_ADV_EVT_WHITELIST_REQUEST:
    NRF_LOG_INFO("BLE_ADV_EVT_WHITELIST_REQUEST.");
    {
      ble_gap_addr_t whitelist_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
      ble_gap_irk_t  whitelist_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
      uint32_t       addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
      uint32_t       irk_cnt  = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

      err_code = pm_whitelist_get(whitelist_addrs, &addr_cnt,
          whitelist_irks,  &irk_cnt);
      APP_ERROR_CHECK(err_code);
      NRF_LOG_INFO("pm_whitelist_get returns %d addr in whitelist and %d irk whitelist",
          addr_cnt,
          irk_cnt);

      // Apply the whitelist.
      err_code = ble_advertising_whitelist_reply(&m_advertising,
          whitelist_addrs,
          addr_cnt,
          whitelist_irks,
          irk_cnt);
      APP_ERROR_CHECK(err_code);
    }
    break;

  case BLE_ADV_EVT_PEER_ADDR_REQUEST:
    NRF_LOG_INFO("BLE_ADV_EVT_PEER_ADDR_REQUEST.");
    {
      pm_peer_data_bonding_t peer_bonding_data;

      // Only Give peer address if we have a handle to the bonded peer.
      if (m_peer_id != PM_PEER_ID_INVALID)
      {

        err_code = pm_peer_data_bonding_load(m_peer_id, &peer_bonding_data);
        if (err_code != NRF_ERROR_NOT_FOUND)
        {
          APP_ERROR_CHECK(err_code);

          ble_gap_addr_t * p_peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
          err_code = ble_advertising_peer_addr_reply(&m_advertising, p_peer_addr);
          APP_ERROR_CHECK(err_code);
        }

      }
      break;
    }

  default:
    NRF_LOG_INFO("%s default.", __func__);
    break;
  }
}

static void
ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
  ret_code_t err_code = NRF_SUCCESS;

  switch (p_ble_evt->header.evt_id)
  {
  case BLE_GAP_EVT_DISCONNECTED:
    NRF_LOG_INFO("Disconnected.");
    // LED indication will be changed when advertising starts.
    break;

  case BLE_GAP_EVT_CONNECTED:
    NRF_LOG_INFO("Connected.");
    err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
    APP_ERROR_CHECK(err_code);
    m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
    {
      NRF_LOG_INFO("PHY update request.");
      ble_gap_phys_t const phys =
      {
        .rx_phys = BLE_GAP_PHY_AUTO,
        .tx_phys = BLE_GAP_PHY_AUTO,
      };
      err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
      APP_ERROR_CHECK(err_code);
    } break;

  case BLE_GATTC_EVT_TIMEOUT:
    // Disconnect on GATT Client timeout event.
    NRF_LOG_INFO("GATT Client Timeout.");
    err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
        BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GATTS_EVT_TIMEOUT:
    // Disconnect on GATT Server timeout event.
    NRF_LOG_INFO("GATT Server Timeout.");
    err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
        BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    APP_ERROR_CHECK(err_code);
    break;

  default:
    NRF_LOG_INFO("hit the default case. %x", p_ble_evt->header.evt_id);
    // No implementation needed.
    break;
  }
}

static void
ble_stack_init(void)
{
  ret_code_t err_code;

  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  // Configure the BLE stack using the default settings.
  // Fetch the start address of the application RAM.
  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  // Enable BLE stack.
  err_code = nrf_sdh_ble_enable(&ram_start);
  APP_ERROR_CHECK(err_code);

  // Register a handler for BLE events.
  NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

static void
peer_manager_init(void)
{
  ble_gap_sec_params_t sec_param;
  ret_code_t           err_code;

  err_code = pm_init();
  APP_ERROR_CHECK(err_code);

  memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

  // Security parameters to be used for all security procedures.
  sec_param.bond           = SEC_PARAM_BOND;
  sec_param.mitm           = SEC_PARAM_MITM;
  sec_param.lesc           = SEC_PARAM_LESC;
  sec_param.keypress       = SEC_PARAM_KEYPRESS;
  sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
  sec_param.oob            = SEC_PARAM_OOB;
  sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
  sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
  sec_param.kdist_own.enc  = 1;
  sec_param.kdist_own.id   = 1;
  sec_param.kdist_peer.enc = 1;
  sec_param.kdist_peer.id  = 1;

  err_code = pm_sec_params_set(&sec_param);
  APP_ERROR_CHECK(err_code);

  err_code = pm_register(pm_evt_handler);
  APP_ERROR_CHECK(err_code);
}

static void
delete_bonds(void)
{
  ret_code_t err_code;

  NRF_LOG_INFO("Erase bonds!");

  err_code = pm_peers_delete();
  APP_ERROR_CHECK(err_code);
}

static void
scheduler_init(void)
{
  APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

static void
bsp_event_handler(bsp_event_t event)
{
  ret_code_t err_code;

  switch (event)
  {
  case BSP_EVENT_SLEEP:
    NRF_LOG_INFO("BSP_EVENT_SLEEP");
    sleep_mode_enter();
    break; // BSP_EVENT_SLEEP

  case BSP_EVENT_DISCONNECT:
    NRF_LOG_INFO("BSP_EVENT_DISCONNECT");
    err_code = sd_ble_gap_disconnect(m_conn_handle,
        BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err_code != NRF_ERROR_INVALID_STATE)
    {
      APP_ERROR_CHECK(err_code);
    }
    break; // BSP_EVENT_DISCONNECT

  case BSP_EVENT_WHITELIST_OFF:
    NRF_LOG_INFO("BSP_EVENT_WHITELIST_OFF");
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
      err_code = ble_advertising_restart_without_whitelist(&m_advertising);
      if (err_code != NRF_ERROR_INVALID_STATE)
      {
        APP_ERROR_CHECK(err_code);
      }
    }
    break; // BSP_EVENT_KEY_0

  default:
    NRF_LOG_INFO("%s default.", __func__);
    break;
  }
}

static void
advertising_init(void)
{
  ret_code_t             err_code;
  ble_advertising_init_t init;

  memset(&init, 0, sizeof(init));

  init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
  init.advdata.include_appearance      = true;
  init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
  init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
  init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

  init.config.ble_adv_fast_enabled  = true;
  init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
  init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

  init.evt_handler = on_adv_evt;

  err_code = ble_advertising_init(&m_advertising, &init);
  APP_ERROR_CHECK(err_code);

  ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void
buttons_leds_init(bool * p_erase_bonds)
{
  ret_code_t err_code;
  bsp_event_t startup_event;

  err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
  APP_ERROR_CHECK(err_code);

  err_code = bsp_btn_ble_init(NULL, &startup_event);
  APP_ERROR_CHECK(err_code);

  *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


static void
log_init(void)
{
  ret_code_t err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);

  NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void
power_management_init(void)
{
  ret_code_t err_code;
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);
}


static void
idle_state_handle(void)
{
  app_sched_execute();
  if (NRF_LOG_PROCESS() == false)
  {
    nrf_pwr_mgmt_run();
  }
}

static void
peer_list_get(pm_peer_id_t * p_peers, uint32_t * p_size)
{
  pm_peer_id_t peer_id;
  uint32_t     peers_to_copy;

  peers_to_copy = (*p_size < BLE_GAP_WHITELIST_ADDR_MAX_COUNT) ?
    *p_size : BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

  peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
  *p_size = 0;

  while ((peer_id != PM_PEER_ID_INVALID) && (peers_to_copy--))
  {
    p_peers[(*p_size)++] = peer_id;
    peer_id = pm_next_peer_id_get(peer_id);
  }
}

static void
advertising_start(bool erase_bonds)
{
  if(erase_bonds == true)
  {
    delete_bonds();
    // Advertising is started by PM_EVT_PEERS_DELETED_SUCEEDED event
  }
  else
  {
    // hkim start
    ret_code_t ret;

    memset(m_whitelist_peers, PM_PEER_ID_INVALID, sizeof(m_whitelist_peers));
    m_whitelist_peer_cnt = (sizeof(m_whitelist_peers) / sizeof(pm_peer_id_t));

    peer_list_get(m_whitelist_peers, &m_whitelist_peer_cnt);

    ret = pm_whitelist_set(m_whitelist_peers, m_whitelist_peer_cnt);
    APP_ERROR_CHECK(ret);

    // Setup the device identies list.
    // Some SoftDevices do not support this feature.
    ret = pm_device_identities_list_set(m_whitelist_peers, m_whitelist_peer_cnt);
    if (ret != NRF_ERROR_NOT_SUPPORTED)
    {
      APP_ERROR_CHECK(ret);
    }
    // hkim end

    ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
  }
}

static void
btn_control_state_changed(uint8_t state)
{
  NRF_LOG_INFO("%s called", __func__);
  ble_hkim_ledbtns_update_btn(&m_ledbtns, state, m_btn_noti_enabled);
}

int
main(void)
{
  bool erase_bonds;
  uint8_t   btn_state;

  // Initialize.
  log_init();

  NRF_LOG_INFO("main begin.");

  timers_init();
  buttons_leds_init(&erase_bonds);
  power_management_init();
  ble_stack_init();
  scheduler_init();
  gap_params_init();
  gatt_init();
  services_init();
  advertising_init();
  conn_params_init();
  peer_manager_init();

  led_control_init();
  btn_state = btn_control_init(btn_control_state_changed);
  ble_hkim_ledbtns_update_btn(&m_ledbtns, btn_state, false);

  // Start execution.
  NRF_LOG_INFO("Template example started.");
  application_timers_start();

  advertising_start(erase_bonds);

  // Enter main loop.
  for (;;)
  {
    idle_state_handle();
  }
}
