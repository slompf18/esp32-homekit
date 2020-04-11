#include "unity.h"

#include "../../src/crypto/hk_hkdf.h"
#include "../../src/include/hk_mem.h"
#include "../../src/utils/hk_logging.h"

const char key_in_bytes[] = {
    0xf7, 0x7f, 0xf3, 0xd0, 0x2a, 0x58, 0x9a, 0x27, 0x09, 0x36, 0x0f, 0xf4, 0x4f, 0xd4, 0x24, 0x47,
    0x47, 0xa9, 0x33, 0xab, 0x90, 0x72, 0x72, 0x54, 0xdc, 0x9a, 0x64, 0xd7, 0x78, 0xc2, 0x1a, 0x04};

const char key_out_bytes[] = {
    0xa7, 0x3f, 0x5c, 0x83, 0xb7, 0xe8, 0x87, 0xdf, 0xf9, 0xf8, 0x77, 0x99, 0x32, 0xca, 0x09, 0x01,
    0x4a, 0x2d, 0xb1, 0x58, 0xc8, 0xdb, 0xcb, 0xf3, 0x89, 0xeb, 0xba, 0x53, 0x4c, 0x8c, 0xb4, 0x95};

TEST_CASE("hkdf", "[crypto] [hkdf]")
{
    // prepare
    hk_mem *key_in = hk_mem_init();
    hk_mem_append_buffer(key_in, (void *)key_in_bytes, 32);
    hk_mem *key_out = hk_mem_init();
    hk_mem *key_out_expected = hk_mem_init();
    hk_mem_append_buffer(key_out_expected, (void *)key_out_bytes, 32);

    // run
    esp_err_t ret = hk_hkdf(key_in, key_out, HK_HKDF_PAIR_SETUP_ACCESSORY_SALT, HK_HKDF_PAIR_SETUP_ACCESSORY_INFO);

    // assert
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(hk_mem_equal(key_out_expected, key_out));

    // clean
    hk_mem_free(key_in);
    hk_mem_free(key_out);
    hk_mem_free(key_out_expected);
}