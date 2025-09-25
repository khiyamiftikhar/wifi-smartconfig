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
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "smartconfig_example";

//If record is already trieed for connection then try smartconfig, otherwise first try using record

static bool storage_connect_tried=false; 
static bool storage_connect_success=false; 



static struct {
    wifi_connect_success_callback callback;

}wifi_state={0};

static void smartconfig_example_task(void * parm);




/// @brief Scan live access points and provide their  details so that it can be checked later whether they are suitable for connction
/// @param ap_records 
/// @param ap_count 
/// @return 
esp_err_t scan_live_wifi_access_points(wifi_ap_record_t* ap_records,uint16_t ap_count){
        
        
        wifi_scan_config_t scan_config = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = true
        };  
        esp_err_t err_check=esp_wifi_scan_start(&scan_config, true);    //This value is stored in dynamically allocated mem
        
        err_check=esp_wifi_scan_get_ap_records(&ap_count, ap_records); //gets data from the previous scan
    return err_check;
}


static void wifi_connect_to_ap(uint8_t* ssid,uint8_t*password,uint8_t* bssid){

    //smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
    wifi_config_t wifi_config;
    //uint8_t ssid[33] = { 0 };
    //uint8_t password[65] = { 0 };
    //uint8_t rvd_data[33] = { 0 };

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

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
}



static void stored_ssid_connection_attemp(){

    uint16_t ap_count = 3;
    wifi_ap_record_t ap_records[ap_count];          //Live scan records
    scan_live_wifi_access_points(ap_records,ap_count);       //Scan for access points and get results
                                                                //in ap_records array

    //This is not correct. The loop should actually run equal to total number of records which can be less  thaan max
    ap_info_t ap_info={0};          //Record from the storafe
    int index;
    for(uint8_t i=0;i<ap_count;i++){
        
        if(ap_records_find_by_ssid((const char*)ap_records[i].ssid, &ap_info, &index)==ESP_OK){
            //Use the live AP SSID, password from the record and live ap bssid
            wifi_connect_to_ap(ap_records[i].ssid,ap_info.password,ap_records[i].bssid);
            return;
        }

    }       

    //if not found then attemp connection by any wronng password so that disconnect event occurs
    //This could crash bcausse there maynot be even a single record
    uint8_t wrong_password[]={1,2,3,4,5,6,7,8};
    ESP_LOGI(TAG,"Wrong pass attempted to init the process anyway");
    wifi_connect_to_ap(ap_records[0].ssid,wrong_password,NULL);
}


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data){

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {

        //First check if can be connected to any available AP because it is in record
        //If no record found then proceed to smartconfig

        storage_connect_tried=true; 
        storage_connect_success=false; 

        //Try to  connect using local record
        stored_ssid_connection_attemp();
                                                           
    }

        
     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {

        //If previously success in connecting from local record, then again use local
        if(storage_connect_success == true)
            stored_ssid_connection_attemp();

        else {   //Try smartconfig
            xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        }
        
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    
    }else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    
        
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        
        if(storage_connect_tried==true)
            storage_connect_success=true;
        //Now inform the user that connection complete
        if(wifi_state.callback!=NULL)
            wifi_state.callback();
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);


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
        ap_records_add((const char*) wifi_config.sta.ssid, (const char*) wifi_config.sta.password, NULL);   //add to the array
        ap_records_save();              //save to non volatile
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
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

esp_err_t initialise_wifi(wifi_smartconfig_t* config){
    ESP_ERROR_CHECK( nvs_flash_init() );    //Should be removed if main initalizes it
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
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
    return 0;
}

static void smartconfig_example_task(void * parm){
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

