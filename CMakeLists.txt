idf_component_register(SRCS "smartconfig.c" ap_record.c blob_storage.c
                        INCLUDE_DIRS .
                        PRIV_REQUIRES esp_wifi wpa_supplicant nvs_flash
                        )