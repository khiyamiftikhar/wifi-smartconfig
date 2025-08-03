/* ap_records.h */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AP information structure
 */
typedef struct {
    uint8_t ssid[33];                       ///< SSID of the AP (null-terminated)
    uint8_t password[65];                   ///< Password of the AP (null-terminated)
    uint8_t bssid[6];                       ///< MAC address of the AP
    uint8_t use_count;                      ///< How many times used. Used for LRU replacement
} ap_info_t;

/**
 * @brief AP records collection structure
 */
typedef struct {
    ap_info_t ap_list[CONFIG_MAX_AP_COUNT];  
    uint8_t available_records;                 ///< Total records currently available
} ap_record_t;

/**
 * @brief Initialize AP records manager
 * @return ESP_OK on success
 */
esp_err_t ap_records_init(void);

/**
 * @brief Load AP records from persistent storage
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no saved data
 */
esp_err_t ap_records_load(void);

/**
 * @brief Save AP records to persistent storage
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ap_records_save(void);

/**
 * @brief Set the entire AP records structure
 * @param records Pointer to ap_record_t structure
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if records is NULL
 */
esp_err_t ap_records_set_all(const ap_record_t* records);

/**
 * @brief Get the entire AP records structure
 * @param records Pointer to store the ap_record_t structure
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if records is NULL
 */
esp_err_t ap_records_get_all(ap_record_t* records);

/**
 * @brief Get pointer to internal AP records (read-only access)
 * @return Pointer to internal ap_record_t structure, NULL if not initialized
 */
const ap_record_t* ap_records_get_readonly(void);

/**
 * @brief Add a new AP record
 * @param ssid SSID of the AP
 * @param password Password of the AP
 * @param bssid BSSID (MAC address) of the AP (can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ap_records_add(const char* ssid, const char* password, const uint8_t* bssid);

/**
 * @brief Get AP record by index
 * @param index Index of the record (0 to ap_records_get_count()-1)
 * @param ap_info Pointer to store the AP info
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if index is invalid
 */
esp_err_t ap_records_get(int index, ap_info_t* ap_info);

/**
 * @brief Get number of stored AP records
 * @return Number of records, or -1 if not initialized
 */
int ap_records_get_count(void);

/**
 * @brief Find AP record by SSID
 * @param ssid SSID to search for
 * @param ap_info Pointer to store the AP info (can be NULL if only checking existence)
 * @param index Pointer to store the index (can be NULL)
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not found
 */
esp_err_t ap_records_find_by_ssid(const char* ssid, ap_info_t* ap_info, int* index);

/**
 * @brief Find AP record by BSSID
 * @param bssid BSSID to search for (6 bytes)
 * @param ap_info Pointer to store the AP info (can be NULL if only checking existence)
 * @param index Pointer to store the index (can be NULL)
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not found
 */
esp_err_t ap_records_find_by_bssid(const uint8_t* bssid, ap_info_t* ap_info, int* index);

/**
 * @brief Increment use count for an AP
 * @param ssid SSID of the AP that was used
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if AP not found
 */
esp_err_t ap_records_increment_use_count(const char* ssid);

/**
 * @brief Remove AP record by SSID
 * @param ssid SSID of the AP to remove
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not found
 */
esp_err_t ap_records_remove_by_ssid(const char* ssid);

/**
 * @brief Remove AP record by index
 * @param index Index of the record to remove
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if index is invalid
 */
esp_err_t ap_records_remove_by_index(int index);

/**
 * @brief Clear all AP records
 * @return ESP_OK on success
 */
esp_err_t ap_records_clear_all(void);

/**
 * @brief Sort AP records by use count (most used first)
 * @return ESP_OK on success
 */
esp_err_t ap_records_sort_by_usage(void);

/**
 * @brief Get size of ap_record_t structure (for storage purposes)
 * @return Size in bytes
 */
size_t ap_records_get_size(void);

/**
 * @brief Print all AP records for debugging
 */
void ap_records_print_all(void);

#ifdef __cplusplus
}
#endif

