#ifndef __BTN_CONTROL_DEF_H__
#define __BTN_CONTROL_DEF_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_soc.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "app_error.h"
#include "boards.h"

typedef void (*btn_control_callback)(uint8_t state);
extern uint8_t btn_control_init(btn_control_callback cb);

#endif /* !__BTN_CONTROL_DEF_H__ */
