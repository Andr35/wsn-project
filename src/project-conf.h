/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* Disable button shutdown functionality */
#define BUTTON_SENSOR_CONF_ENABLE_SHUTDOWN 0
/*---------------------------------------------------------------------------*/
/* Enable the ROM bootloader */
#define ROM_BOOTLOADER_ENABLE 1
/*---------------------------------------------------------------------------*/
/* Change to match your configuration */
#define IEEE802154_CONF_PANID 0xABCD
#define RF_CORE_CONF_CHANNEL 26
#define RF_BLE_CONF_ENABLED 0
/*---------------------------------------------------------------------------*/

/* Choose the next first two lines or the third */

#undef NETSTACK_RDC
#define NETSTACK_RDC nullrdc_driver
// #define NETSTACK_RDC contikimac_driver


#define NULLRDC_802154_AUTOACK 1
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
