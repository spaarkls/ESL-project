#include "cli.h"

#include <stddef.h>

#include "defines.h"

#define READ_SIZE  ( 1 )

static char m_rx_buffer[READ_SIZE];

static void usb_ev_handler(app_usbd_class_inst_t const * p_inst, app_usbd_cdc_acm_user_event_t event);

#define CDC_ACM_COMM_INTERFACE  ( 2 )
#define CDC_ACM_COMM_EPIN       ( NRF_DRV_USBD_EPIN3 )

#define CDC_ACM_DATA_INTERFACE  ( 3 )
#define CDC_ACM_DATA_EPIN       ( NRF_DRV_USBD_EPIN4 )
#define CDC_ACM_DATA_EPOUT      ( NRF_DRV_USBD_EPOUT4 )

APP_USBD_CDC_ACM_GLOBAL_DEF(usb_cdc_acm,
                            usb_ev_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE);


static void usbd_user_ev_handler(app_usbd_event_type_t event) {
    switch (event)
    {
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            app_usbd_start();
            break;
        default:
            break;
    }
}


void usb_init(void) {
    ret_code_t ret;
#if 0
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };
    ret = app_usbd_init(&usbd_config);
#endif

    app_usbd_class_inst_t const * class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&usb_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    // ret = app_usbd_power_events_enable();
    // APP_ERROR_CHECK(ret);
}


void usb_process(void) {
    app_usbd_event_queue_process();

}

static void usb_ev_handler(app_usbd_class_inst_t const * p_inst, app_usbd_cdc_acm_user_event_t event) {
    switch (event) {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN: {
            ret_code_t ret;
            ret = app_usbd_cdc_acm_read(&usb_cdc_acm, m_rx_buffer, READ_SIZE);
            UNUSED(ret);
            break;
        }
    
        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE: {
            ret_code_t ret;
            ret = app_usbd_cdc_acm_read(&usb_cdc_acm, m_rx_buffer, READ_SIZE);
            UNUSED(ret);
            break;
        }

        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE: {
            break;   
        }

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
            ret_code_t ret;
            do {
                size_t size = app_usbd_cdc_acm_rx_size(&usb_cdc_acm);
                if (m_rx_buffer[0] == '\r' || m_rx_buffer[0] == '\n') {
                    ret = app_usbd_cdc_acm_write(&usb_cdc_acm, "\r\n", 2);
                
                } else {
                    ret = app_usbd_cdc_acm_write(&usb_cdc_acm, m_rx_buffer, READ_SIZE);    
                }

                ret = app_usbd_cdc_acm_read(&usb_cdc_acm, m_rx_buffer, READ_SIZE);

            } while (ret == NRF_SUCCESS);
            
            break;
        }

        default:
            break;
    }
}