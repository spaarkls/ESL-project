#ifndef CLI_H_
#define CLI_H_

#include <stdint.h>
#include <stdbool.h>


#include "app_usbd_core.h"
#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"


void usb_init(void);
void usb_process(void);


#endif // CLI_H_