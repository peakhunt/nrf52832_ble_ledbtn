#include "led_control.h"

static nrf_drv_gpiote_pin_t   _led_pins[] =
{
  20,
  19,
  18,
  17,
  16,
  15
};

void
led_control_init(void)
{
  // init gpio
  for(int i = 0; i < sizeof(_led_pins)/sizeof(nrf_drv_gpiote_pin_t); i++)
  {
    nrf_gpio_cfg_output(_led_pins[i]);
  }
}

void
led_control_set(uint8_t status)
{
  for(int i = 0; i < sizeof(_led_pins)/sizeof(nrf_drv_gpiote_pin_t); i++)
  {
    if((status & (0x01 << i)))
    {
      // on
      nrf_gpio_pin_set(_led_pins[i]);
    }
    else
    {
      // off
      nrf_gpio_pin_clear(_led_pins[i]);
    }
  }
}
