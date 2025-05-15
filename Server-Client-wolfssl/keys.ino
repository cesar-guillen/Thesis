#include <Arduino.h>
#include "mbedtls/ecdh.h"
#include "mbedtls/ecp.h"
#include <mbedtls/error.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

const uint8_t private_key[KEY_SIZE] = {
  0xC7, 0x10, 0xCC, 0x02, 0x7D, 0x3F, 0x28, 0x58, 
  0x50, 0x61, 0xFD, 0x18, 0x9F, 0x62, 0x0C, 0x55, 
  0xA7, 0x60, 0x3E, 0x22, 0xD2, 0xE3, 0x8B, 0xCF, 
  0xC6, 0x39, 0xD5, 0x8F, 0xA6, 0xBB, 0xF2, 0x51
};

const uint8_t peer_pubkey[65] = {
  0x04, 0x6C, 0xE5, 0x65, 0x7A, 0x1B, 0xE9, 0x83, 
  0x2E, 0x53, 0x5F, 0xA2, 0x48, 0x41, 0x73, 0x5A, 
  0xDC, 0x22, 0xF6, 0xB6, 0x1B, 0x4A, 0x5F, 0x99, 
  0x81, 0xF5, 0x0D, 0xE5, 0xAB, 0x8E, 0xE0, 0xC7, 
  0x8C, 0x5A, 0x6E, 0x5D, 0xC7, 0xBB, 0x43, 0x99, 
  0x66, 0x13, 0x31, 0xC9, 0x6C, 0xB6, 0x38, 0x9C, 
  0x1C, 0xFC, 0xDB, 0x19, 0x62, 0x10, 0x80, 0x70, 
  0x81, 0x90, 0xDB, 0xAF, 0xE7, 0xA1, 0x36, 0x6D, 
  0x21
};

void generate_shared_secret() {
    mbedtls_ecp_group grp;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point Qp;
    mbedtls_ecp_point_init(&Qp);
    mbedtls_mpi d;  // private key
    mbedtls_mpi z;  // shared key 
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    uint8_t shared_secret[KEY_SIZE];
    size_t secret_len;

    int ret = 0;

    const mbedtls_ecp_group_id grp_id = MBEDTLS_ECP_DP_SECP256R1;
    ret = mbedtls_ecp_group_load(&grp, grp_id);
    if (ret != 0) {
        Serial.println("Failed to load curve group");
        goto cleanup;
    }

    // Initialize the RNG
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) {
        Serial.println("Failed to initialize RNG");
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary(&d, private_key, KEY_SIZE);
    if (ret != 0) {
        Serial.println("Failed to read private key");
        goto cleanup;
    }

    if (mbedtls_mpi_cmp_int(&d, 1) < 0 || mbedtls_mpi_cmp_mpi(&d, &grp.N) >= 0) {
        Serial.println("Private key is out of range");
        goto cleanup;
    }

    // load other  public key into Qp
    ret = mbedtls_ecp_point_read_binary(&grp, &Qp, peer_pubkey, sizeof(peer_pubkey));
    if (ret != 0) {
        Serial.println("Failed to load peer public key");
        goto cleanup;
    }

    // Compute shared secret: z = (peer_pubkey)^private_key
    ret = mbedtls_ecdh_compute_shared(&grp, &z, &Qp, &d, mbedtls_ctr_drbg_random, &ctr_drbg);
    secret_len = mbedtls_mpi_size(&z);
    ret = mbedtls_mpi_write_binary(&z, shared_secret, secret_len);
    if (ret != 0) {
        Serial.println("Failed to export shared secret");
        goto cleanup;
    }

    memcpy(key, (const void*)shared_secret, KEY_SIZE);

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Qp);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&z);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
