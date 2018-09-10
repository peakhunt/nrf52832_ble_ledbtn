#ifndef __BLE_HKIM_LEDS_H__
#define __BLE_HKIM_LEDS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

// f364adc9-b000-4042-ba50-05ca45bf8abc
#define BLE_UUID_HKIM_LEDBTN_BASE             {0xBC, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, \
                                              0x40, 0x42, 0xB0, 0x00, 0xC9, 0xAD, 0x64, 0xF3}

#define BLE_UUID_HKIM_LEDBTN_SERVICE          0x1400

#define BLE_UUID_HKIM_LEDBTN_LEDS_CHAR        0x1401
#define BLE_UUID_HKIM_LEDBTN_BTNS_CHAR        0x1402

#define BLE_HKIM_LEDBTNS_OBSERVER_PRIO        2

#define BLE_HKIM_LEDBTNS_DEF(_name)                                       \
static ble_hkim_ledbtns_t _name;                                          \
NRF_SDH_BLE_OBSERVER(_name ## _obs,                                       \
                     BLE_HKIM_LEDBTNS_OBSERVER_PRIO,                      \
                     ble_hkim_ledbtns_on_ble_evt, &_name)

typedef enum
{
  BLE_HKIM_LEDBTNS_EVT_LED_UPDATED,
  BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_ENABLED,
  BLE_HKIM_LEDBTNS_EVT_BTN_NOTIFICATION_DISABLED,
  BLE_HKIM_LEDBTNS_EVT_CONNECTED,
  BLE_HKIM_LEDBTNS_EVT_DISCONNECTED,
} ble_hkim_ledbtns_evt_type_t;

typedef struct
{
  ble_hkim_ledbtns_evt_type_t evt_type;
} ble_hkim_ledbtns_evt_t;

typedef struct ble_hkim_ledbtns_s ble_hkim_ledbtns_t;

typedef void (*ble_hkim_ledbtns_evt_handle_t)(ble_hkim_ledbtns_t* p_hkim_btnleds, ble_hkim_ledbtns_evt_t* p_evt);

typedef struct
{
  ble_hkim_ledbtns_evt_handle_t     evt_handler;
  ble_srv_cccd_security_mode_t      hkim_leds_value_char_attr_md;
  ble_srv_cccd_security_mode_t      hkim_btns_value_char_attr_md;
} ble_hkim_ledbtns_init_t;

struct ble_hkim_ledbtns_s
{
  ble_hkim_ledbtns_evt_handle_t     evt_handler;
  uint16_t                          service_handle;
  uint16_t                          conn_handle;
  uint8_t                           uuid_type;

  ble_gatts_char_handles_t          leds_handles;
  ble_gatts_char_handles_t          btns_handles;

  uint8_t                           leds_status;
  uint8_t                           btns_status;
};

uint32_t ble_hkim_ledbtns_init(ble_hkim_ledbtns_t* p_hkim_btnleds, ble_hkim_ledbtns_init_t const* p_hkim_leds_init);
void ble_hkim_ledbtns_on_gatt_evt(ble_hkim_ledbtns_t* p_hkim_btnleds, nrf_ble_gatt_evt_t const * p_gatt_evt);
void ble_hkim_ledbtns_on_ble_evt(ble_evt_t const* p_ble_evt, void*  p_context);
int ble_hkim_ledbtns_update_btn(ble_hkim_ledbtns_t* p_hkim_btnleds, uint8_t status);

#ifdef __cplusplus
}
#endif

#endif /* !__BLE_HKIM_LEDS_H__ */
