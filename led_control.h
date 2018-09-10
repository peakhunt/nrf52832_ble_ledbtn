#ifndef __LED_CONTROL_DEF_H__
#define __LED_CONTROL_DEF_H__

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

extern void led_control_init(void);
extern void led_control_set(uint8_t status);

#endif /* !__LED_CONTROL_DEF_H__ */
