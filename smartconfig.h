#ifndef SMARTCONFIG_H
#define SMARTCONFIG_H

#include "esp_err.h"

typedef void (*wifi_connect_success_callback)(void);


typedef struct{
    //Calls when connection success. added because espnow requires it
    wifi_connect_success_callback callback;

}wifi_smartconfig_t;


esp_err_t initialise_wifi(wifi_smartconfig_t* config);



#endif