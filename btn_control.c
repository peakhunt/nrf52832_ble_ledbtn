#include "btn_control.h"
#include "app_scheduler.h"
#include "ble_hkim_ledbtns.h"

static nrf_drv_gpiote_pin_t   _btn_pins[] = 
{
  14,
};

static volatile uint8_t                 _pin_state = 0;
static volatile btn_control_callback    _cb;

static void
pin_state_change_handler(void* p_event_data, uint16_t event_size)
{
  uint8_t state = *(uint8_t*)p_event_data;

  _cb(state);
}

static inline int
get_pin_ndx(nrf_drv_gpiote_pin_t pin)
{
  for(int i = 0; i < sizeof(_btn_pins)/sizeof(nrf_drv_gpiote_pin_t); i++)
  {
    if(pin == _btn_pins[i])
    {
      return i;
    }
  }
  APP_ERROR_CHECK(-1);

  return -1;
}

static void
in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  uint32_t  hi = nrf_gpio_pin_read(pin);
  int       ndx = get_pin_ndx(pin);
  uint8_t   state;

  if(hi)
  {
    _pin_state |= (1 << ndx);
  }
  else
  {
    _pin_state &= ~(1 << ndx);
  }

  state = _pin_state;

  //
  // does it really have to be executed in mainloop context?
  // not from IRQ context?
  // dunno.
  //
  app_sched_event_put(&state, sizeof(state), pin_state_change_handler);
}

uint8_t
btn_control_init(btn_control_callback cb)
{
  ret_code_t  err_code;
  uint32_t    pin_state;

  _cb = cb;

#if 0
  err_code = nrf_drv_gpiote_init();
  APP_ERROR_CHECK(err_code);
#endif

  for(int i = 0; i < sizeof(_btn_pins)/sizeof(nrf_drv_gpiote_pin_t); i++)
  {
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    //in_config.pull = NRF_GPIO_PIN_PULLUP;
    in_config.pull = NRF_GPIO_PIN_PULLDOWN;
  
    err_code = nrf_drv_gpiote_in_init(_btn_pins[i], &in_config, in_pin_handler);
    APP_ERROR_CHECK(err_code);

  }

  for(int i = 0; i < sizeof(_btn_pins)/sizeof(nrf_drv_gpiote_pin_t); i++)
  {
    pin_state = nrf_gpio_pin_read(_btn_pins[i]);
    if(pin_state)
    {
      _pin_state |= (1 << i);
    }
  }

  for(int i = 0; i < sizeof(_btn_pins)/sizeof(nrf_drv_gpiote_pin_t); i++)
  {
    nrf_drv_gpiote_in_event_enable(_btn_pins[i], true);
  }

  return _pin_state;
}
