#ifndef SMARTCONFIG_H
#define SMARTCONFIG_H

#include "esp_err.h"

typedef void (*wifi_connect_success_callback)(void);


typedef struct{
    //Calls when connection success. added because espnow requires it
    wifi_connect_success_callback callback;
    bool power_save;

}wifi_smartconfig_t;

/// @brief Set the attempt to reconnect on a disconnect to true or false. if set false it will not try to reconnect
/// @param reconnect 
void wifi_set_reconnect(bool reconnect);
esp_err_t wifi_initialize(wifi_smartconfig_t* config);



#endif