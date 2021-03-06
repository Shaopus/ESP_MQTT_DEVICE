#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USE_OPTIMIZE_PRINTF


#define PLUG_DEVICE             1
#define ENVSENSOR_DEVICE		0


#if PLUG_DEVICE
#define SWITCH_GPIO 12
#define SWITCH_GPIO_MUX PERIPHS_IO_MUX_MTDI_U
#define SWITCH_GPIO_FUNC FUNC_GPIO12
#define SWITCH_INVERT false

#define BUTTON_CONNECTED true
#define BUTTON_GPIO 0
#define BUTTON_GPIO_MUX PERIPHS_IO_MUX_GPIO0_U
#define BUTTON_GPIO_FUNC FUNC_GPIO0
#endif

#if ENVSENSOR_DEVICE
#define DHT_GPIO 2
#define DHT_GPIO_MUX PERIPHS_IO_MUX_GPIO2_U
#define DHT_GPIO_FUNC FUNC_GPIO2
#define DHT_DELAY 5000	/* milliseconds */
#endif

//#define SWITCH_GPIO 12
//#define SWITCH_GPIO_MUX PERIPHS_IO_MUX_MTDI_U
//#define SWITCH_GPIO_FUNC FUNC_GPIO12
//
//#define BUTTON_GPIO 0
//#define BUTTON_GPIO_MUX PERIPHS_IO_MUX_GPIO0_U
//#define BUTTON_GPIO_FUNC FUNC_GPIO0

#endif

