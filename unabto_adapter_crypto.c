// Nabto includes
#include <unabto/unabto_aes_cbc.h>
#include <unabto/unabto_hmac_sha256.h>
#include <unabto/util/unabto_buffer.h>
#include <unabto/unabto_util.h>

// Standard includes
#include <stdlib.h>

// Driverlib includes
#include "rom_map.h"
#include "hw_memmap.h"
#include "shamd5.h"
#include "aes.h"

volatile bool g_aesContextReady;
volatile bool g_shaContextReady;

void AESIntHandler(void) {
    if (MAP_AESIntStatus(AES_BASE, true) & AES_INT_CONTEXT_IN) {
        MAP_AESIntDisable(AES_BASE, AES_INT_CONTEXT_IN);
        g_aesContextReady = true;
    }
}

void SHAMD5IntHandler(void) {
    if (MAP_SHAMD5IntStatus(SHAMD5_BASE, true) & SHAMD5_INT_CONTEXT_READY) {
        MAP_SHAMD5IntDisable(SHAMD5_BASE, SHAMD5_INT_CONTEXT_READY);
        g_shaContextReady = true;
    }
}

bool aes128_cbc_crypt(const uint8_t *key, uint8_t *input, uint16_t input_len,
                      uint32_t config) {
    if ((input_len < 16) || (input_len % 16 != 0)) {
        return false;
    }

    // separate iv and data
    uint8_t *iv = input;
    input += 16;
    input_len -= 16;

    // wait for context to be ready
    g_aesContextReady = false;
    MAP_AESIntEnable(AES_BASE, AES_INT_CONTEXT_IN);
    while (!g_aesContextReady);

    // configure and encrypt/decrypt
    MAP_AESConfigSet(AES_BASE,
                     config | AES_CFG_MODE_CBC | AES_CFG_KEY_SIZE_128BIT);
    MAP_AESIVSet(AES_BASE, iv);
    MAP_AESKey1Set(AES_BASE, (uint8_t *)key, AES_CFG_KEY_SIZE_128BIT);
    MAP_AESDataProcess(AES_BASE, input, input, input_len);

    return true;
}

bool aes128_cbc_encrypt(const uint8_t *key, uint8_t *input,
                        uint16_t input_len) {
    return aes128_cbc_crypt(key, input, input_len, AES_CFG_DIR_ENCRYPT);
}

bool aes128_cbc_decrypt(const uint8_t *key, uint8_t *input,
                        uint16_t input_len) {
    return aes128_cbc_crypt(key, input, input_len, AES_CFG_DIR_DECRYPT);
}

uint8_t tmp_buf[2048];

void unabto_hmac_sha256_buffers(const buffer_t keys[], uint8_t keys_size,
                                const buffer_t messages[],
                                uint8_t messages_size, uint8_t *mac,
                                uint16_t mac_size) {
    uint8_t i;

    // concatenate keys
    uint16_t key_size = 0;
    for (i = 0; i < keys_size; i++) {
        key_size += keys[i].size;
    }
    UNABTO_ASSERT(key_size <= UNABTO_SHA256_BLOCK_LENGTH);
    uint8_t key[512];
    memset(key, 0, 512);
    uint8_t *key_ptr = key;
    for (i = 0; i < keys_size; i++) {
        memcpy(key_ptr, keys[i].data, keys[i].size);
        key_ptr += keys[i].size;
    }

    // concatenate messages
    uint16_t message_size = 0;
    for (i = 0; i < messages_size; i++) {
        message_size += messages[i].size;
    }
    //uint8_t *message = (uint8_t *)malloc(message_size);
    uint8_t *message = tmp_buf;
    uint8_t *message_ptr = message;
    for (i = 0; i < messages_size; i++) {
        memcpy(message_ptr, messages[i].data, messages[i].size);
        message_ptr += messages[i].size;
    }

    // wait for context to be ready
    g_shaContextReady = false;
    MAP_SHAMD5IntEnable(SHAMD5_BASE, SHAMD5_INT_CONTEXT_READY);
    while (!g_shaContextReady) {
    }

    // configure and generate hash
    MAP_SHAMD5ConfigSet(SHAMD5_BASE, SHAMD5_ALGO_HMAC_SHA256);

    MAP_SHAMD5HMACKeySet(SHAMD5_BASE, key);

    uint8_t tmp_mac[32]; // To cope with possible truncated mac
    MAP_SHAMD5HMACProcess(SHAMD5_BASE, message, message_size, tmp_mac);
    memcpy(mac, tmp_mac, mac_size) ;


    //free(message);
}
