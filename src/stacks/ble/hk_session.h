#pragma once

#include <host/ble_hs.h>

#include "../../include/hk_mem.h"
#include "../../common/hk_chrs_properties.h"

typedef struct
{
    int srv_index;
    char srv_id;
    int chr_index;
    uint16_t instance_id;
    bool srv_primary;
    bool srv_hidden;
    bool srv_supports_configuration;
} hk_session_setup_info_t;

typedef struct
{
    uint8_t transaction_id;
    uint8_t last_opcode;
    const char* static_data;
    esp_err_t (*read_callback)(hk_mem* response);
    esp_err_t (*write_callback)(hk_mem* request, hk_mem* response);
    char srv_index;
    char srv_id;
    bool srv_primary;
    bool srv_hidden;
    bool srv_supports_configuration;
    const ble_uuid128_t* srv_uuid;
    char chr_index;
    hk_chr_types_t chr_type;
    float max_length;
    float min_length;
    hk_mem *request;
    hk_mem *request_encrypted;
    hk_mem *response;
    hk_mem *response_encrypted;
    uint16_t request_length;
    uint16_t response_sent;
    uint8_t response_status;
    uint16_t connection_handle;
} hk_session_t;

hk_session_t* hk_session_init(hk_chr_types_t chr_type, hk_session_setup_info_t *hk_gatt_setup_info);