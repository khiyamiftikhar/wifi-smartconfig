/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_mac.h"
#include  "ap_record.h"
#include  "smartconfig.h"

#define     MAX_SCANNED_AP                  6
#define     WIFI_RECONNECT_ATTEMPTS         3
#define     ERR_SSID_NOT_FOUND                  -99
/* FreeRTOS event group to signal when we are connected & ready to make a request */
#define WIFI_API_CALL_PROCEED_CHECK(label)                  \
    do {                                                    \
        wifi_mode_t mode;                                   \
        esp_err_t err = esp_wifi_get_mode(&mode);           \
        if (err != ESP_OK) {                                \
            ESP_LOGI(TAG, "came here in disconnect if");    \
            goto label;                                     \
        }                                                   \
    } while (0)






/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

static const int WIFI_EVENT_INIT_BIT = BIT0;
static const int WIFI_EVENT_CONNECTED_BIT = BIT1;
static const int WIFI_EVENT_DISCONNECTED_BIT = BIT2;

static const int WIFI_EVENT_ESPTOUCH_DONE_BIT = BIT3;
static const char *TAG = "smartconfig_example";

//If record is already trieed for connection then try smartconfig, otherwise first try using record

static bool storage_connect_tried=false; 
static bool storage_connect_success=false; 


typedef enum{
    WIFI_STATE_INIT=0,
    WIFI_STATE_ATTEMPT_SMARTCONFIG,
    WIFI_STATE_CONNECTED, 

}wifi_protocol_state_t;



static struct {
    TaskHandle_t wifi_task_handle;
    EventGroupHandle_t wifi_event_group;
    wifi_connect_success_callback callback;
    bool attemp_reconnect;
    wifi_protocol_state_t state;
    

}wifi_state={0};

//static void smartconfig_example_task(void * parm);
static void wifi_task(void* args);



/// @brief Scan live access points and provide their  details so that it can be checked later whether they are suitable for connction
/// @param ap_records 
/// @param ap_count 
/// @return 
static esp_err_t scan_live_wifi_access_points(wifi_ap_record_t* ap_records,uint16_t* ap_count){
        
        
        wifi_scan_config_t scan_config = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = true
        };  
        esp_err_t err_check=esp_wifi_scan_start(&scan_config, true);    //This value is stored in dynamically allocated mem
        
        err_check=esp_wifi_scan_get_ap_records(ap_count, ap_records); //gets data from the previous scan
    return err_check;
}


static esp_err_t wifi_connect_to_ap(uint8_t* ssid,uint8_t*password,uint8_t* bssid){


    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

/*
#ifdef CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            ESP_LOGI(TAG, "Set MAC address of target AP: "MACSTR" ", MAC2STR(evt->bssid));
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
#endif
*/
                
    ESP_LOGI(TAG, "SSID:%s", ssid);
    ESP_LOGI(TAG, "PASSWORD:%s", password);
        /*
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }*/

        //Checks if wifi api is initalized before proceeding to the calls
    /*ESP_ERROR_CHECK(*/esp_wifi_disconnect();//);
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    return esp_wifi_connect();
       
}



static esp_err_t stored_ssid_connection_attempt(wifi_ap_record_t* ap_scanned){


    //This is not correct. The loop should actually run equal to total number of records which can be less  thaan max
    ap_info_t ap_record={0};          //Record from the storafe
    int index=0;
    
        
    if(ap_records_find_by_ssid((const char*)ap_scanned->ssid, &ap_record, &index)==ESP_OK){
        //Use the live AP SSID, password from the record and live ap bssid
        return wifi_connect_to_ap(ap_scanned->ssid,ap_record.password,ap_scanned->bssid);
    }
    

    return ERR_SSID_NOT_FOUND;
}


static esp_err_t stored_ssid_delete_record(wifi_ap_record_t* ap_scanned){

    return ap_records_remove_by_ssid((const char*)ap_scanned->ssid);

}


static esp_err_t add_success_ssid_record(const char* ssid,const char* password){

    esp_err_t ret=0;
    if(ap_records_add(ssid,password,NULL)==ESP_OK)
        ret=ap_records_save();              //save to non volatile

    return ret;

}





static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data){

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {

        //First check if can be connected to any available AP because it is in record
        //If no record found then proceed to smartconfig

        //The task is waiting for this signal
        xEventGroupSetBits(wifi_state.wifi_event_group,WIFI_EVENT_INIT_BIT);

        storage_connect_tried=true; 
        storage_connect_success=false; 
    }

        
     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {


        xEventGroupSetBits(wifi_state.wifi_event_group,WIFI_EVENT_DISCONNECTED_BIT);
                
        xEventGroupClearBits(wifi_state.wifi_event_group, WIFI_EVENT_CONNECTED_BIT);
    
    }else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        
        
        
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        
        if(storage_connect_tried==true)
            storage_connect_success=true;
        //Now inform the user that connection complete
        if(wifi_state.callback!=NULL)
            wifi_state.callback();
        xEventGroupSetBits(wifi_state.wifi_event_group, WIFI_EVENT_CONNECTED_BIT);


    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

 
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        //This is not optimum as user may have supplied the wrong password
        //So it should only be added when connection is successful, but right how
        //how to get this ssid and oassat that event
        
#ifdef CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            ESP_LOGI(TAG, "Set MAC address of target AP: "MACSTR" ", MAC2STR(evt->bssid));
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
#endif

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));

        
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_LOGI(TAG, "Setting config");
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(wifi_state.wifi_event_group, WIFI_EVENT_ESPTOUCH_DONE_BIT);
    }
}



void wifi_set_reconnect(bool reconnect){


    wifi_state.attemp_reconnect=reconnect;
}

esp_err_t wifi_initialize(wifi_smartconfig_t* config){


    

    ESP_ERROR_CHECK( nvs_flash_init() );    //Should be removed if main initalizes it
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_state.wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    if(!config->power_save)
        ESP_ERROR_CHECK( esp_wifi_set_ps(WIFI_PS_NONE) );


    ESP_LOGI(TAG,"wifi task creating");
    BaseType_t err= xTaskCreate(wifi_task, "wifi task", 4096, NULL, 5, &wifi_state.wifi_task_handle);

    ESP_LOGI(TAG,"wifi task creation done");

    ESP_ERROR_CHECK(err==pdFAIL);
    

    esp_err_t ret=0;
    ret=ap_records_init();


    if(ret==ESP_ERR_NOT_FOUND){
        ESP_ERROR_CHECK(ap_records_save());

    }
    else if(ret!=ESP_OK){
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(ap_records_load());

    if(config!=NULL)
        wifi_state.callback=config->callback;
    else
        wifi_state.callback=NULL;

       
    ap_records_print_all();

    //by default it is true;
    wifi_state.attemp_reconnect=true;
    return 0;
}



static esp_err_t wifi_stored_ap_record_connect(){

    esp_err_t ret=0;
    wifi_ap_record_t ap_scan_results[MAX_SCANNED_AP];          //Live scan records
    uint16_t ap_count=MAX_SCANNED_AP;
    //uint8_t reconnect_attempts=WIFI_RECONNECT_ATTEMPTS;
    EventBits_t uxBits;
    //As input parameter it tells the size of record array, and as output it tells the actual number read
    scan_live_wifi_access_points(ap_scan_results,&ap_count);       //Scan for access points and get results
    
    if(ap_count>0){
        for(uint8_t i=0;i<ap_count;i++){
            
            for(uint8_t j=0;j<WIFI_RECONNECT_ATTEMPTS;j++){
                ret=stored_ssid_connection_attempt(&ap_scan_results[i]);
                
                //If not found then break from this loop, and try next ssid
                if(ret==ERR_SSID_NOT_FOUND){
                    break;
                }
                //If some other reason of not sucess, then skip wait and try again
                else if(ret!=ESP_OK){
                    continue;
                }

                //if success then wait for any of the events to occur
                uxBits=xEventGroupWaitBits(wifi_state.wifi_event_group,
                                WIFI_EVENT_CONNECTED_BIT|WIFI_EVENT_DISCONNECTED_BIT,
                                pdTRUE,pdFALSE,portMAX_DELAY);

                if(uxBits&WIFI_EVENT_CONNECTED_BIT){
                    return ESP_OK;
                }
            }
                //If it doesn exit , it means that record has wrong password , so remove that record
                stored_ssid_delete_record(&ap_scan_results[i]);

        }

    }

    return ESP_FAIL;
}


static esp_err_t wifi_smartconfig_connect(){
    
    esp_err_t ret=0;
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    
    uxBits = xEventGroupWaitBits(wifi_state.wifi_event_group, WIFI_EVENT_DISCONNECTED_BIT | WIFI_EVENT_ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
    
    /*
    if(uxBits & WIFI_EVENT_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi Connected to ap");
        //If success then add it to record
        wifi_config_t wifi_config;
        ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
        
        //If success then add the ssid record to NVS
        if(ret==ESP_OK){
            add_success_ssid_record(wifi_config.sta.ssid,wifi_config.sta.password);
        }
        return ESP_OK;
    }*/
    
    if(uxBits & WIFI_EVENT_DISCONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi DisConnected frim ap");
        //If success then add it to record
        
        return ESP_FAIL;
    }
    
    if(uxBits & WIFI_EVENT_ESPTOUCH_DONE_BIT) {
        ESP_LOGI(TAG, "smartconfig over");
        wifi_config_t wifi_config;
        
        ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
        
        //If success then add the ssid record to NVS
        if(ret==ESP_OK){
            add_success_ssid_record((const char*)wifi_config.sta.ssid,(const char*)wifi_config.sta.password);
        }
        esp_smartconfig_stop();
        return ESP_OK;
    
    }

    return ESP_FAIL;

}

static void wifi_task(void* args){

    EventBits_t uxBits;
    esp_err_t ret=0;
    wifi_protocol_state_t next_state=0;
    while(1){
            switch(wifi_state.state){

                case WIFI_STATE_INIT:
                    uxBits=xEventGroupWaitBits(wifi_state.wifi_event_group,WIFI_EVENT_INIT_BIT,pdTRUE,pdTRUE,portMAX_DELAY);

                    if (uxBits & WIFI_EVENT_INIT_BIT) {

                        ret=wifi_stored_ap_record_connect();
                        if(ret==ESP_OK){
                            next_state=WIFI_STATE_CONNECTED;
                        }
                        else{
                            next_state=WIFI_STATE_ATTEMPT_SMARTCONFIG;
                        }
                    }
                    break;
                    
                    
                case WIFI_STATE_ATTEMPT_SMARTCONFIG:
                    ret=wifi_smartconfig_connect();
                            if(ret==ESP_OK){
                                    next_state=WIFI_STATE_CONNECTED;
                    
                            }
                    break;

                case WIFI_STATE_CONNECTED:
                    uxBits=xEventGroupWaitBits(wifi_state.wifi_event_group,WIFI_EVENT_DISCONNECTED_BIT,pdTRUE,pdTRUE,portMAX_DELAY);
                    if (uxBits & WIFI_EVENT_DISCONNECTED_BIT) {
                        if(wifi_state.attemp_reconnect==true)
                            next_state=WIFI_STATE_INIT;
                    }

                    break;

                default:
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    break;

            }


                wifi_state.state=next_state;

    }




}
