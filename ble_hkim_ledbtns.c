#include "sdk_common.h"
#include "ble_hkim_ledbtns.h"
#include <string.h>
#include "ble_srv_common.h"

#include "nrf_log.h"

////////////////////////////////////////////////////////////////////////////////
//
// module privates
//
////////////////////////////////////////////////////////////////////////////////
static uint32_t
hkim_btnleds_char_add(ble_hkim_ledbtns_t* p_hkim_btnleds, const ble_hkim_ledbtns_init_t* p_hkim_leds_init)
{
  int err_code;
  ble_gatts_char_md_t char_md;
  ble_gatts_attr_t    attr_char_value;
  ble_uuid_t          ble_uuid;
  ble_gatts_attr_md_t attr_md;

  ble_gatts_attr_md_t cccd_md;

  memset(&cccd_md, 0, sizeof(cccd_md));

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

  cccd_md.write_perm = p_hkim_leds_init->hkim_leds_value_char_attr_md.cccd_write_perm;
  cccd_md.vloc       = BLE_GATTS_VLOC_STACK;

  memset(&char_md, 0, sizeof(char_md));

  //
  // LED characteristic
  //
  char_md.char_props.read     = 1;
  char_md.char_props.write    = 1;
  char_md.char_props.notify   = 0; 
  char_md.p_char_user_desc    = NULL;
  char_md.p_char_pf           = NULL;
  char_md.p_user_desc_md      = NULL;
  char_md.p_cccd_md           = NULL;
  char_md.p_sccd_md           = NULL;

  ble_uuid.type = p_hkim_btnleds->uuid_type;
  ble_uuid.uuid = BLE_UUID_HKIM_LEDBTN_LEDS_CHAR;

  memset(&attr_md, 0, sizeof(attr_md));

  attr_md.read_perm  = p_hkim_leds_init->hkim_leds_value_char_attr_md.read_perm;
  attr_md.write_perm = p_hkim_leds_init->hkim_leds_value_char_attr_md.write_perm;
  attr_md.vloc       = BLE_GATTS_VLOC_STACK;
  attr_md.rd_auth    = 0;
  attr_md.wr_auth    = 0;
  attr_md.vlen       = 0;

  memset(&attr_char_value, 0, sizeof(attr_char_value));

  attr_char_value.p_uuid    = &ble_uuid;
  attr_char_value.p_attr_md = &attr_md;
  attr_char_value.init_len  = sizeof(uint8_t);
  attr_char_value.init_offs = 0;
  attr_char_value.max_len   = sizeof(uint8_t);
  attr_char_value.p_value   = &p_hkim_btnleds->leds_status;

  err_code = sd_ble_gatts_characteristic_add(p_hkim_btnleds->service_handle,
                                             &char_md,
                                             &attr_char_value,
                                             &p_hkim_btnleds->leds_handles);
  if(err_code != NRF_SUCCESS)
  {
    NRF_LOG_INFO("sd_ble_gatts_characteristic_add failed for leds");
    return err_code;
  }

  //
  // Button characteristic
  //
  char_md.char_props.read     = 1;
  char_md.char_props.write    = 0;
  char_md.char_props.notify   = 1; 
  char_md.p_char_user_desc    = NULL;
  char_md.p_char_pf           = NULL;
  char_md.p_user_desc_md      = NULL;
  char_md.p_cccd_md           = &cccd_md;
  char_md.p_sccd_md           = NULL;

  ble_uuid.type = p_hkim_btnleds->uuid_type;
  ble_uuid.uuid = BLE_UUID_HKIM_LEDBTN_BTNS_CHAR;

  memset(&attr_md, 0, sizeof(attr_md));

  attr_md.read_perm  = p_hkim_leds_init->hkim_btns_value_char_attr_md.read_perm;
  attr_md.write_perm = p_hkim_leds_init->hkim_btns_value_char_attr_md.write_perm;
  attr_md.vloc       = BLE_GATTS_VLOC_STACK;
  attr_md.rd_auth    = 0;
  attr_md.wr_auth    = 0;
  attr_md.vlen       = 0;

  memset(&attr_char_value, 0, sizeof(attr_char_value));

  attr_char_value.p_uuid    = &ble_uuid;
  attr_char_value.p_attr_md = &attr_md;
  attr_char_value.init_len  = sizeof(uint8_t);
  attr_char_value.init_offs = 0;
  attr_char_value.max_len   = sizeof(uint8_t);
  attr_char_value.p_value   = &p_hkim_btnleds->btns_status;

  err_code = sd_ble_gatts_characteristic_add(p_hkim_btnleds->service_handle,
                                             &char_md,
                                             &attr_char_value,
                                             &p_hkim_btnleds->btns_handles);
  if(err_code != NRF_SUCCESS)
  {
    NRF_LOG_INFO("sd_ble_gatts_characteristic_add failed for buttons");
    return err_code;
  }

  return NRF_SUCCESS;
}

static void
on_hkim_leds_value_write(ble_hkim_ledbtns_t* p_hkim_btnleds, ble_gatts_evt_write_t const * p_evt_write)
{
  if(p_evt_write->len != 1)
  {
    NRF_LOG_INFO("%s p_evt_write->len != 1", __func__);
    return;
  }

  if (p_hkim_btnleds->evt_handler != NULL)
  {
    ble_hkim_ledbtns_evt_t evt;

    evt.evt_type = BLE_HKIM_LEDBTNS_EVT_LED_UPDATED;
    p_hkim_btnleds->leds_status = p_evt_write->data[0];
    p_hkim_btnleds->evt_handler(p_hkim_btnleds, &evt);
  }
}

static void
on_hkim_btns_cccd_write(ble_hkim_ledbtns_t* p_hkim_btnleds, ble_gatts_evt_write_t const * p_evt_write)
{
  ble_hkim_ledbtns_evt_t evt;

  if(p_evt_write->len != 2)
  {
    NRF_LOG_INFO("%s p_evt_write->len is not 2", __func__);
    return;
  }

  if(ble_srv_is_notification_enabled(p_evt_write->data))
  {
    evt.evt_type = BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_ENABLED;
  }
  else
  {
    evt.evt_type = BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_DISABLED;
  }
  p_hkim_btnleds->evt_handler(p_hkim_btnleds, &evt);
}

static void
on_connect(ble_hkim_ledbtns_t* p_hkim_btnleds, ble_evt_t const * p_ble_evt)
{
  ble_hkim_ledbtns_evt_t evt;

  p_hkim_btnleds->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

  evt.evt_type = BLE_HKIM_LEDBTNS_EVT_CONNECTED;
  p_hkim_btnleds->evt_handler(p_hkim_btnleds, &evt);
}

static void
on_disconnect(ble_hkim_ledbtns_t * p_hkim_btnleds, ble_evt_t const * p_ble_evt)
{
  ble_hkim_ledbtns_evt_t evt;

  UNUSED_PARAMETER(p_ble_evt);

  p_hkim_btnleds->conn_handle = BLE_CONN_HANDLE_INVALID;

  evt.evt_type = BLE_HKIM_LEDBTNS_EVT_DISCONNECTED;
  p_hkim_btnleds->evt_handler(p_hkim_btnleds, &evt);
}

static void
on_write(ble_hkim_ledbtns_t * p_hkim_btnleds, ble_evt_t const * p_ble_evt)
{
  ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

  if(p_evt_write->handle == p_hkim_btnleds->leds_handles.value_handle)
  {
    //
    // LED value control
    //
    on_hkim_leds_value_write(p_hkim_btnleds, p_evt_write);
  }
  else if(p_evt_write->handle == p_hkim_btnleds->btns_handles.cccd_handle)
  {
    //
    // Button CCCD control
    //
    on_hkim_btns_cccd_write(p_hkim_btnleds, p_evt_write);
  }
  else
  {
    NRF_LOG_INFO("%s unknown case", __func__);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// module publics
//
////////////////////////////////////////////////////////////////////////////////
uint32_t
ble_hkim_ledbtns_init(ble_hkim_ledbtns_t* p_hkim_btnleds, ble_hkim_ledbtns_init_t const* p_hkim_leds_init)
{
  uint32_t        err_code;
  ble_uuid_t      ble_uuid;
  ble_uuid128_t   base_uuid = {BLE_UUID_HKIM_LEDBTN_BASE};

  p_hkim_btnleds->evt_handler                = p_hkim_leds_init->evt_handler;
  p_hkim_btnleds->conn_handle                = BLE_CONN_HANDLE_INVALID;
  p_hkim_btnleds->leds_status                = 0;

  err_code = sd_ble_uuid_vs_add(&base_uuid, &p_hkim_btnleds->uuid_type);
  if(err_code != NRF_SUCCESS)
  {
    NRF_LOG_INFO("sd_ble_uuid_vs_add failed.");
    return err_code;
  }

  // BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_HKIM_LEDBTN_SERVICE);
  ble_uuid.type = p_hkim_btnleds->uuid_type;
  ble_uuid.uuid = BLE_UUID_HKIM_LEDBTN_SERVICE;

  err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                      &ble_uuid,
                                      &p_hkim_btnleds->service_handle);

  if(err_code != NRF_SUCCESS)
  {
    NRF_LOG_INFO("sd_ble_gatts_service_add failed.");
    return err_code;
  }

  err_code = hkim_btnleds_char_add(p_hkim_btnleds, p_hkim_leds_init);
  if(err_code != NRF_SUCCESS)
  {
    NRF_LOG_INFO("hkim_btnleds_char_add failed.");
    return err_code;
  }

  return NRF_SUCCESS;
}

void
ble_hkim_ledbtns_on_gatt_evt(ble_hkim_ledbtns_t* p_hkim_btnleds, nrf_ble_gatt_evt_t const * p_gatt_evt)
{
  // XXX
  //
  // nothing to do now. MTU update???
  //
}

void
ble_hkim_ledbtns_on_ble_evt(ble_evt_t const* p_ble_evt, void*  p_context)
{
  ble_hkim_ledbtns_t* p_hkim_btnleds = (ble_hkim_ledbtns_t*)p_context;

  switch (p_ble_evt->header.evt_id)
  {
  case BLE_GAP_EVT_CONNECTED:
    NRF_LOG_INFO("XXX BLE_GAP_EVT_CONNECTED");
    on_connect(p_hkim_btnleds, p_ble_evt);
    break;

  case BLE_GAP_EVT_DISCONNECTED:
    NRF_LOG_INFO("XXX BLE_GAP_EVT_DISCONNECTED");
    on_disconnect(p_hkim_btnleds, p_ble_evt);
    break;

  case BLE_GATTS_EVT_WRITE:
    NRF_LOG_INFO("XXX BLE_GATTS_EVT_WRITE");
    on_write(p_hkim_btnleds, p_ble_evt);
    break;

  default:
    NRF_LOG_INFO("XXX default %x", p_ble_evt->header.evt_id);
    // No implementation needed.
    break;
  }
}

int
ble_hkim_ledbtns_update_btn(ble_hkim_ledbtns_t* p_hkim_btnleds, uint8_t status)
{
  int err_code;
  ble_gatts_value_t   gatts_value;

  NRF_LOG_INFO("%s %02x\n", __func__, status);

  p_hkim_btnleds->btns_status = status;
  
  memset(&gatts_value, 0, sizeof(gatts_value));
  gatts_value.len       = sizeof(uint8_t);
  gatts_value.offset    = 0;
  gatts_value.p_value   = &status;

  // update database
  err_code = sd_ble_gatts_value_set(p_hkim_btnleds->conn_handle,
                                    p_hkim_btnleds->btns_handles.value_handle,
                                    &gatts_value);

  if(err_code != NRF_SUCCESS)
  {
    return err_code;
  }

  //
  // notify if connected
  // ble_hkim_ledbtns_update_btn is called by app only when
  // notification is enabled!!!
  //
  if(p_hkim_btnleds->conn_handle != BLE_CONN_HANDLE_INVALID)
  {
    ble_gatts_hvx_params_t hvx_params;
    
    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_hkim_btnleds->btns_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = gatts_value.offset;
    hvx_params.p_len  = &gatts_value.len;
    hvx_params.p_data = gatts_value.p_value;

    err_code = sd_ble_gatts_hvx(p_hkim_btnleds->conn_handle, &hvx_params);
    NRF_LOG_INFO("sd_ble_gatts_hvx result: %x. \r\n", err_code);
  }
  else
  {
    err_code = NRF_ERROR_INVALID_STATE;
    NRF_LOG_INFO("%s conn_handle is invalid", __func__);
  }

  return err_code;
}
