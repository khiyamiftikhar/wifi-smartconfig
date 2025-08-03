/* ap_records.c */
#include "ap_record.h"
#include "blob_storage.h"  // Internal dependency - not exposed to users
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "string.h"
#include "nvs.h"

static const char *TAG = "AP_RECORDS";

// Storage configuration - internal to this component
#define AP_RECORDS_NAMESPACE "ap_storage"
#define AP_RECORDS_KEY "ap_records"

// Static instance - only this component manages it
static ap_record_t ap_records = {0};
static bool is_initialized = false;
static blob_storage_handle_t storage_handle = {0};

esp_err_t ap_records_init(void)
{
    if (is_initialized) {
        ESP_LOGD(TAG, "AP records already initialized");
        return ESP_OK;
    }

    // Clear the structure
    memset(&ap_records, 0, sizeof(ap_record_t));
    
    // Initialize blob storage system
    esp_err_t ret = blob_storage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize blob storage: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create storage handle for AP records
    ret = blob_storage_create_handle(&storage_handle, 
                                   AP_RECORDS_NAMESPACE, 
                                   AP_RECORDS_KEY, 
                                   sizeof(ap_record_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create storage handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    is_initialized = true;
    ESP_LOGI(TAG, "AP records manager initialized");
    
    // Automatically try to load existing data
    ret = ap_records_load();
    if (ret == ESP_ERR_NOT_FOUND) {
        
        ESP_LOGI(TAG, "No existing AP records found, starting fresh");
        
        return ret;
    } else if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load AP records: %s", esp_err_to_name(ret));
    }
    
    return ESP_OK;
}

esp_err_t ap_records_load(void)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t size = sizeof(ap_record_t);
    esp_err_t ret = blob_storage_read(&storage_handle, &ap_records, &size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "No AP records found in storage");
        // Initialize with empty records
        memset(&ap_records, 0, sizeof(ap_record_t));
        return ESP_ERR_NOT_FOUND;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read AP records: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Validate loaded data
    if (size != sizeof(ap_record_t)) {
        ESP_LOGW(TAG, "Size mismatch in stored data, resetting");
        memset(&ap_records, 0, sizeof(ap_record_t));
        return ESP_ERR_INVALID_SIZE;
    }
    
    if (ap_records.available_records > CONFIG_MAX_AP_COUNT) {
        ESP_LOGW(TAG, "Invalid record count in stored data, resetting");
        memset(&ap_records, 0, sizeof(ap_record_t));
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGI(TAG, "Loaded %d AP records from storage", ap_records.available_records);
    return ESP_OK;
}

esp_err_t ap_records_save(void)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = blob_storage_write(&storage_handle, &ap_records, sizeof(ap_record_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save AP records: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Saved %d AP records to storage", ap_records.available_records);
    return ESP_OK;
}

esp_err_t ap_records_set_all(const ap_record_t* records)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!records) {
        ESP_LOGE(TAG, "Invalid records pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate record count
    if (records->available_records > CONFIG_MAX_AP_COUNT) {
        ESP_LOGE(TAG, "Invalid record count: %d (max: %d)", records->available_records, CONFIG_MAX_AP_COUNT);
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&ap_records, records, sizeof(ap_record_t));
    ESP_LOGD(TAG, "Set %d AP records", ap_records.available_records);
    return ESP_OK;
}

esp_err_t ap_records_get_all(ap_record_t* records)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!records) {
        ESP_LOGE(TAG, "Invalid records pointer");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(records, &ap_records, sizeof(ap_record_t));
    return ESP_OK;
}

const ap_record_t* ap_records_get_readonly(void)
{
    if (!is_initialized) {
        return NULL;
    }
    return &ap_records;
}

esp_err_t ap_records_add(const char* ssid, const char* password, const uint8_t* bssid)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid || !password) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG,"SSID %s, pass %s",ssid,password);
    if (strlen(ssid) >= sizeof(ap_records.ap_list[0].ssid) || 
        strlen(password) >= sizeof(ap_records.ap_list[0].password)) {
        ESP_LOGE(TAG, "SSID or password too long");
        return ESP_ERR_INVALID_ARG;
    }

    // Check if SSID already exists
    for (int i = 0; i < ap_records.available_records; i++) {
        if (strcmp((char*)ap_records.ap_list[i].ssid, ssid) == 0) {
            // Update existing record
            strncpy((char*)ap_records.ap_list[i].password, password, sizeof(ap_records.ap_list[i].password) - 1);
            ap_records.ap_list[i].password[sizeof(ap_records.ap_list[i].password) - 1] = '\0';
            
            if (bssid) {
                memcpy(ap_records.ap_list[i].bssid, bssid, 6);
            }
            ap_records.ap_list[i].use_count++;
            
            ESP_LOGI(TAG, "Updated existing AP record: %s", ssid);
            return ESP_OK;
        }
    }

    // Add new record
    if (ap_records.available_records < CONFIG_MAX_AP_COUNT) {
        // Add to available slot
        int index = ap_records.available_records;
        strncpy((char*)ap_records.ap_list[index].ssid, ssid, sizeof(ap_records.ap_list[index].ssid) - 1);
        ap_records.ap_list[index].ssid[sizeof(ap_records.ap_list[index].ssid) - 1] = '\0';
        
        strncpy((char*)ap_records.ap_list[index].password, password, sizeof(ap_records.ap_list[index].password) - 1);
        ap_records.ap_list[index].password[sizeof(ap_records.ap_list[index].password) - 1] = '\0';
        
        if (bssid) {
            memcpy(ap_records.ap_list[index].bssid, bssid, 6);
        } else {
            memset(ap_records.ap_list[index].bssid, 0, 6);
        }
        
        ap_records.ap_list[index].use_count = 1;
        ap_records.available_records++;
        
        ESP_LOGI(TAG, "Added new AP record: %s (total: %d)", ssid, ap_records.available_records);
    } else {
        // Find record with lowest use_count to replace
        int min_use_index = 0;
        for (int i = 1; i < CONFIG_MAX_AP_COUNT; i++) {
            if (ap_records.ap_list[i].use_count < ap_records.ap_list[min_use_index].use_count) {
                min_use_index = i;
            }
        }
        
        // Replace the least used record
        ESP_LOGI(TAG, "Replacing least used AP: %s (use count: %d) with: %s", 
                 ap_records.ap_list[min_use_index].ssid, ap_records.ap_list[min_use_index].use_count, ssid);
        
        strncpy((char*)ap_records.ap_list[min_use_index].ssid, ssid, sizeof(ap_records.ap_list[min_use_index].ssid) - 1);
        ap_records.ap_list[min_use_index].ssid[sizeof(ap_records.ap_list[min_use_index].ssid) - 1] = '\0';
        
        strncpy((char*)ap_records.ap_list[min_use_index].password, password, sizeof(ap_records.ap_list[min_use_index].password) - 1);
        ap_records.ap_list[min_use_index].password[sizeof(ap_records.ap_list[min_use_index].password) - 1] = '\0';
        
        if (bssid) {
            memcpy(ap_records.ap_list[min_use_index].bssid, bssid, 6);
        } else {
            memset(ap_records.ap_list[min_use_index].bssid, 0, 6);
        }
        
        ap_records.ap_list[min_use_index].use_count = 1;
    }

    return ESP_OK;
}

esp_err_t ap_records_get(int index, ap_info_t* ap_info)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ap_info || index < 0 || index >= ap_records.available_records) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(ap_info, &ap_records.ap_list[index], sizeof(ap_info_t));
    return ESP_OK;
}

int ap_records_get_count(void)
{
    if (!is_initialized) {
        return -1;
    }
    return ap_records.available_records;
}

esp_err_t ap_records_find_by_ssid(const char* ssid, ap_info_t* ap_info, int* index)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ap_records.available_records; i++) {
        if (strcmp((char*)ap_records.ap_list[i].ssid, ssid) == 0) {
            if (ap_info) {
                memcpy(ap_info, &ap_records.ap_list[i], sizeof(ap_info_t));
            }
            if (index) {
                *index = i;
            }
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t ap_records_find_by_bssid(const uint8_t* bssid, ap_info_t* ap_info, int* index)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!bssid) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ap_records.available_records; i++) {
        if (memcmp(ap_records.ap_list[i].bssid, bssid, 6) == 0) {
            if (ap_info) {
                memcpy(ap_info, &ap_records.ap_list[i], sizeof(ap_info_t));
            }
            if (index) {
                *index = i;
            }
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t ap_records_increment_use_count(const char* ssid)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ap_records.available_records; i++) {
        if (strcmp((char*)ap_records.ap_list[i].ssid, ssid) == 0) {
            ap_records.ap_list[i].use_count++;
            ESP_LOGD(TAG, "Incremented use count for %s to %d", ssid, ap_records.ap_list[i].use_count);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t ap_records_remove_by_ssid(const char* ssid)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ap_records.available_records; i++) {
        if (strcmp((char*)ap_records.ap_list[i].ssid, ssid) == 0) {
            // Shift remaining records
            for (int j = i; j < ap_records.available_records - 1; j++) {
                memcpy(&ap_records.ap_list[j], &ap_records.ap_list[j + 1], sizeof(ap_info_t));
            }
            ap_records.available_records--;
            
            // Clear the last record
            memset(&ap_records.ap_list[ap_records.available_records], 0, sizeof(ap_info_t));
            
            ESP_LOGI(TAG, "Removed AP record: %s (remaining: %d)", ssid, ap_records.available_records);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t ap_records_remove_by_index(int index)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (index < 0 || index >= ap_records.available_records) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Removing AP record at index %d: %s", index, ap_records.ap_list[index].ssid);

    // Shift remaining records
    for (int i = index; i < ap_records.available_records - 1; i++) {
        memcpy(&ap_records.ap_list[i], &ap_records.ap_list[i + 1], sizeof(ap_info_t));
    }
    ap_records.available_records--;
    
    // Clear the last record
    memset(&ap_records.ap_list[ap_records.available_records], 0, sizeof(ap_info_t));
    
    ESP_LOGI(TAG, "Removed AP record (remaining: %d)", ap_records.available_records);
    return ESP_OK;
}

esp_err_t ap_records_clear_all(void)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    memset(&ap_records, 0, sizeof(ap_record_t));
    ESP_LOGI(TAG, "Cleared all AP records");
    return ESP_OK;
}

esp_err_t ap_records_sort_by_usage(void)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "AP records not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (ap_records.available_records <= 1) {
        return ESP_OK; // Nothing to sort
    }

    // Simple bubble sort by use_count (descending order)
    for (int i = 0; i < ap_records.available_records - 1; i++) {
        for (int j = 0; j < ap_records.available_records - i - 1; j++) {
            if (ap_records.ap_list[j].use_count < ap_records.ap_list[j + 1].use_count) {
                // Swap records
                ap_info_t temp;
                memcpy(&temp, &ap_records.ap_list[j], sizeof(ap_info_t));
                memcpy(&ap_records.ap_list[j], &ap_records.ap_list[j + 1], sizeof(ap_info_t));
                memcpy(&ap_records.ap_list[j + 1], &temp, sizeof(ap_info_t));
            }
        }
    }

    ESP_LOGD(TAG, "Sorted AP records by usage");
    return ESP_OK;
}

size_t ap_records_get_size(void)
{
    return sizeof(ap_record_t);
}

void ap_records_print_all(void)
{
    if (!is_initialized) {
        ESP_LOGW(TAG, "AP records not initialized");
        return;
    }

    ESP_LOGI(TAG, "=== AP Records (Total: %d) ===", ap_records.available_records);
    for (int i = 0; i < ap_records.available_records; i++) {
        ESP_LOGI(TAG, "Record %d: SSID='%s', BSSID=" MACSTR ", Use Count=%d", i, (const char*)ap_records.ap_list[i].ssid, MAC2STR(ap_records.ap_list[i].bssid), ap_records.ap_list[i].use_count);
    }
}