/* blob_storage.h */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Storage handle for managing different blob types
 */
typedef struct {
    char namespace[16];         ///< NVS namespace
    char key[16];              ///< NVS key
    size_t max_size;           ///< Maximum allowed blob size
    bool initialized;          ///< Handle initialization status
} blob_storage_handle_t;

/**
 * @brief Initialize blob storage system
 * @note This must be called after nvs_flash_init()
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t blob_storage_init(void);

/**
 * @brief Create a storage handle for a specific blob type
 * @param handle Pointer to handle structure to initialize
 * @param namespace NVS namespace (max 15 chars)
 * @param key NVS key (max 15 chars)
 * @param max_size Maximum allowed blob size
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t blob_storage_create_handle(blob_storage_handle_t* handle, 
                                    const char* namespace, 
                                    const char* key, 
                                    size_t max_size);

/**
 * @brief Write blob data to NVS
 * @param handle Storage handle
 * @param data Pointer to data to write
 * @param size Size of data in bytes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t blob_storage_write(const blob_storage_handle_t* handle, const void* data, size_t size);

/**
 * @brief Read blob data from NVS
 * @param handle Storage handle
 * @param data Pointer to buffer to store data
 * @param size Pointer to size variable (input: buffer size, output: actual data size)
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if no data, other error codes on failure
 */
esp_err_t blob_storage_read(const blob_storage_handle_t* handle, void* data, size_t* size);

/**
 * @brief Check if blob exists in storage
 * @param handle Storage handle
 * @param exists Pointer to store existence flag
 * @param size Pointer to store blob size (can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t blob_storage_exists(const blob_storage_handle_t* handle, bool* exists, size_t* size);

/**
 * @brief Delete blob from storage
 * @param handle Storage handle
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if no data, other error codes on failure
 */
esp_err_t blob_storage_delete(const blob_storage_handle_t* handle);

/**
 * @brief Get statistics for a storage handle
 * @param handle Storage handle
 * @param used_size Size of stored data (0 if no data)
 * @param max_size Maximum allowed size
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t blob_storage_get_stats(const blob_storage_handle_t* handle, size_t* used_size, size_t* max_size);

/**
 * @brief Check if storage system is initialized
 * @return true if initialized, false otherwise
 */
bool blob_storage_is_initialized(void);

#ifdef __cplusplus
}
#endif
