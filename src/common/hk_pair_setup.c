#include "hk_pair_setup.h"

#include <string.h>

#include "../utils/hk_logging.h"
#include "../utils/hk_tlv.h"
#include "../utils/hk_store.h"
#include "../utils/hk_util.h"
#include "../include/hk_mem.h"
#include "../crypto/hk_srp.h"
#include "../crypto/hk_hkdf.h"
#include "../crypto/hk_ed25519.h"
#include "../crypto/hk_chacha20poly1305.h"
#include "../stacks/hk_advertising.h"

#include "hk_pairings_store.h"
#include "hk_pair_tlvs.h"

hk_mem *hk_pair_setup_public_key;
hk_srp_key_t *hk_pair_setup_srp_key;

void hk_pairing_setup_srp_start(hk_mem *result)
{
    HK_LOGI("Starting pairing");
    HK_LOGD("pairing setup 1/3 (start).");

    esp_err_t ret = ESP_OK;
    hk_pair_setup_public_key = hk_mem_init();
    hk_mem *salt = hk_mem_init();
    hk_tlv_t *tlv_data = NULL;

    // init
    hk_pair_setup_srp_key = hk_srp_init_key();
    ret = hk_srp_generate_key(hk_pair_setup_srp_key, "Pair-Setup", hk_store_code_get()); // username has to be Pair-Setup according specification

    if (!ret)
        ret = hk_srp_export_public_key(hk_pair_setup_srp_key, hk_pair_setup_public_key);

    if (!ret)
        ret = hk_srp_export_salt(hk_pair_setup_srp_key, salt);

    tlv_data = hk_tlv_add_uint8(tlv_data, HK_PAIR_TLV_STATE, HK_PAIR_TLV_STATE_M2);
    if (!ret)
    {
        tlv_data = hk_tlv_add(tlv_data, HK_PAIR_TLV_PUBLICKEY, hk_pair_setup_public_key);
        tlv_data = hk_tlv_add(tlv_data, HK_PAIR_TLV_SALT, salt);
    }
    else
    {
        tlv_data = hk_tlv_add_uint8(tlv_data, HK_PAIR_TLV_ERROR, HK_PAIR_TLV_ERROR_UNKNOWN);
    }

    hk_tlv_serialize(tlv_data, result);

    hk_tlv_free(tlv_data);
    hk_mem_free(salt);

    HK_LOGD("pairing setup 1/3 done.");
}

void hk_pairing_setup_srp_verify(hk_tlv_t *tlv, hk_mem *result)
{
    HK_LOGD("pairing setup 2/3 (verifying).");

    esp_err_t ret = ESP_OK;
    size_t authError = 0;
    hk_mem *ios_pk = hk_mem_init();
    hk_mem *ios_proof = hk_mem_init();
    hk_mem *accessory_proof = hk_mem_init();
    hk_tlv_t *tlv_data = NULL;

    ret = hk_tlv_get_mem_by_type(tlv, HK_PAIR_TLV_PUBLICKEY, ios_pk);

    if (!ret)
        ret = hk_tlv_get_mem_by_type(tlv, HK_PAIR_TLV_PROOF, ios_proof);

    if (!ret)
        ret = hk_srp_compute_key(hk_pair_setup_srp_key, hk_pair_setup_public_key, ios_pk);

    if (!ret)
    {
        ret = hk_srp_verify(hk_pair_setup_srp_key, ios_proof);
        if (ret)
        {
            authError = 1;
        }
    }

    if (!ret)
        ret = hk_srp_export_proof(hk_pair_setup_srp_key, accessory_proof);

    tlv_data = hk_tlv_add_uint8(tlv_data, HK_PAIR_TLV_STATE, HK_PAIR_TLV_STATE_M4);
    if (!ret)
    {
        tlv_data = hk_tlv_add(tlv_data, HK_PAIR_TLV_PROOF, accessory_proof);
    }
    else
    {
        tlv_data = hk_tlv_add_uint8(tlv_data, HK_PAIR_TLV_ERROR, authError ? HK_PAIR_TLV_ERROR_AUTHENTICATION : HK_PAIR_TLV_ERROR_UNKNOWN);
    }

    hk_tlv_serialize(tlv_data, result);

    hk_tlv_free(tlv_data);
    hk_mem_free(hk_pair_setup_public_key);
    hk_mem_free(ios_pk);
    hk_mem_free(ios_proof);
    hk_mem_free(accessory_proof);

    HK_LOGD("pairing setup 2/3 done.");
}

esp_err_t hk_pairing_setup_exchange_response_verification(hk_tlv_t *tlv, hk_mem *shared_secret, hk_mem *srp_private_key, hk_mem *device_id)
{
    esp_err_t ret = ESP_OK;
    hk_mem *encrypted_data = hk_mem_init();
    hk_mem *decrypted_data = hk_mem_init();
    hk_mem *device_ltpk = hk_mem_init();
    hk_mem *device_signature = hk_mem_init();
    hk_mem *device_info = hk_mem_init();
    hk_mem *device_x = hk_mem_init();
    hk_tlv_t *tlv_data_decrypted = NULL;
    hk_ed25519_key_t *device_key = hk_ed25519_init();

    ret = hk_tlv_get_mem_by_type(tlv, HK_PAIR_TLV_ENCRYPTEDDATA, encrypted_data);

    if (!ret)
        ret = hk_hkdf(srp_private_key, shared_secret, HK_HKDF_PAIR_SETUP_ENCRYPT_SALT, HK_HKDF_PAIR_SETUP_ENCRYPT_INFO);

    if (!ret)
        ret = hk_chacha20poly1305_decrypt(shared_secret, HK_CHACHA_SETUP_MSG5, encrypted_data, decrypted_data);

    if (!ret)
        tlv_data_decrypted = hk_tlv_deserialize(decrypted_data);

    if (!ret)
        ret = hk_tlv_get_mem_by_type(tlv_data_decrypted, HK_PAIR_TLV_IDENTIFIER, device_id);

    if (!ret)
    {
        ret = hk_tlv_get_mem_by_type(tlv_data_decrypted, HK_PAIR_TLV_PUBLICKEY, device_ltpk);
    }

    if (!ret)
        ret = hk_tlv_get_mem_by_type(tlv_data_decrypted, HK_PAIR_TLV_SIGNATURE, device_signature);

    if (!ret)
        ret = hk_ed25519_update_from_random_from_public_key(device_key, device_ltpk);

    if (!ret)
        ret = hk_hkdf(srp_private_key, device_x, HK_HKDF_PAIR_SETUP_CONTROLLER_SALT, HK_HKDF_PAIR_SETUP_CONTROLLER_INFO);

    if (!ret)
    {
        hk_mem_append(device_info, device_x);
        hk_mem_append(device_info, device_id);
        hk_mem_append(device_info, device_ltpk);
        ret = hk_ed25519_verify(device_key, device_signature, device_info);
    }

    if (!ret)
    {
        hk_pairings_store_add(device_id, device_ltpk, true);
    }

    hk_tlv_free(tlv_data_decrypted);
    hk_mem_free(encrypted_data);
    hk_mem_free(decrypted_data);
    hk_mem_free(device_ltpk);
    hk_mem_free(device_signature);
    hk_mem_free(device_info);
    hk_mem_free(device_x);
    hk_ed25519_free(device_key);
    hk_srp_free_key(hk_pair_setup_srp_key);

    return ret;
}

esp_err_t hk_pairing_setup_exchange_response_generation(hk_mem *result, hk_mem *shared_secret, hk_mem *srp_private_key)
{
    esp_err_t ret = ESP_OK;
    hk_ed25519_key_t *accessory_key = hk_ed25519_init();
    hk_mem *accessory_public_key = hk_mem_init();
    hk_mem *accessory_private_key = hk_mem_init();
    hk_mem *accessory_info = hk_mem_init();
    hk_mem *accessory_signature = hk_mem_init();
    hk_mem *accessory_id = hk_mem_init();
    hk_mem *sub_result = hk_mem_init();
    hk_mem *encrypted = hk_mem_init();
    hk_tlv_t *tlv_data = NULL;
    hk_tlv_t *sub_tlv_data = NULL;

    if (!ret)
    {
        ret = hk_ed25519_update_from_random(accessory_key);
    }

    if (!ret)
    {
        ret = hk_ed25519_export_public_key(accessory_key, accessory_public_key);
        hk_store_key_pub_set(accessory_public_key);
    }

    if (!ret)
    {
        ret = hk_ed25519_export_private_key(accessory_key, accessory_private_key);
        hk_store_key_priv_set(accessory_private_key);
    }

    if (!ret)
        ret = hk_hkdf(srp_private_key, accessory_info, HK_HKDF_PAIR_SETUP_ACCESSORY_SALT, HK_HKDF_PAIR_SETUP_ACCESSORY_INFO);

    if (!ret)
    {
        hk_util_get_accessory_id_serialized(accessory_id);
        hk_mem_append_buffer(accessory_info, accessory_id->ptr, accessory_id->size);
        hk_mem_append_buffer(accessory_info, accessory_public_key->ptr, accessory_public_key->size);
        ret = hk_ed25519_sign(accessory_key, accessory_info, accessory_signature);
    }

    if (!ret)
    {
        sub_tlv_data = hk_tlv_add(sub_tlv_data, HK_PAIR_TLV_IDENTIFIER, accessory_id);
        sub_tlv_data = hk_tlv_add(sub_tlv_data, HK_PAIR_TLV_PUBLICKEY, accessory_public_key);
        sub_tlv_data = hk_tlv_add(sub_tlv_data, HK_PAIR_TLV_SIGNATURE, accessory_signature);

        hk_tlv_serialize(sub_tlv_data, sub_result);
        hk_chacha20poly1305_encrypt(shared_secret, HK_CHACHA_SETUP_MSG6, sub_result, encrypted);
    }

    tlv_data = hk_tlv_add_uint8(tlv_data, HK_PAIR_TLV_STATE, HK_PAIR_TLV_STATE_M6);
    if (!ret)
    {
        tlv_data = hk_tlv_add(tlv_data, HK_PAIR_TLV_ENCRYPTEDDATA, encrypted);
    }
    else
    {
        tlv_data = hk_tlv_add_uint8(tlv_data, HK_PAIR_TLV_ERROR, HK_PAIR_TLV_ERROR_AUTHENTICATION);
    }

    hk_tlv_serialize(tlv_data, result);

    hk_tlv_free(tlv_data);
    hk_mem_free(accessory_id);
    hk_mem_free(accessory_public_key);
    hk_mem_free(accessory_private_key);
    hk_mem_free(accessory_info);
    hk_mem_free(accessory_signature);
    hk_mem_free(sub_result);
    hk_mem_free(encrypted);
    hk_ed25519_free(accessory_key);

    return ret;
}

void hk_pairing_setup_exchange_response(hk_tlv_t *tlv, hk_mem *result, hk_mem *device_id)
{
    HK_LOGD("pairing setup 3/3 (exchange response).");

    hk_mem *shared_secret = hk_mem_init();
    hk_mem *srp_private_key = hk_mem_init();

    esp_err_t ret = hk_srp_export_private_key(hk_pair_setup_srp_key, srp_private_key);

    if (!ret)
        ret = hk_pairing_setup_exchange_response_verification(tlv, shared_secret, srp_private_key, device_id);

    if (!ret)
        ret = hk_pairing_setup_exchange_response_generation(result, shared_secret, srp_private_key);

    if (!ret)
    {
        hk_advertising_update_paired();
        HK_LOGI("Accessory paired.");
    }

    hk_mem_free(shared_secret);
    hk_mem_free(srp_private_key);

    HK_LOGD("pairing setup 3/3 done.");
}

int hk_pair_setup(hk_mem *request, hk_mem *response, hk_mem *device_id)
{
    hk_tlv_t *tlv_data = hk_tlv_deserialize(request);
    hk_tlv_t *type_tlv = hk_tlv_get_tlv_by_type(tlv_data, HK_PAIR_TLV_STATE);
    int res = HK_RES_OK;

    if (type_tlv == NULL)
    {
        HK_LOGE("Could not find tlv with type state.");
        res = HK_RES_MALFORMED_REQUEST;
    }
    else
    {
        switch (*type_tlv->value)
        {
        case HK_PAIR_TLV_STATE_M1:
            hk_pairing_setup_srp_start(response);
            break;
        case HK_PAIR_TLV_STATE_M3:
            hk_pairing_setup_srp_verify(tlv_data, response);
            break;
        case HK_PAIR_TLV_STATE_M5:
            hk_pairing_setup_exchange_response(tlv_data, response, device_id);
            break;
        default:
            HK_LOGE("Unexpected value in tlv in pair setup: %d", *type_tlv->value);
            res = HK_RES_MALFORMED_REQUEST;
        }
    }

    hk_tlv_free(tlv_data);

    return res;
}