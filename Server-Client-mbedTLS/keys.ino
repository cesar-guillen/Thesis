#include <mbedtls/ecdh.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#define PUB_KEY_BUFFER_SIZE 100

// mbedTLS structures
mbedtls_ecdh_context ecdh_ctx;
mbedtls_entropy_context entropy_ctx;
mbedtls_ctr_drbg_context ctr_drbg_ctx;

byte public_key_buffer[PUB_KEY_BUFFER_SIZE];
bool keys_generated = false;
bool peer_public_key_received = false;

size_t public_key_size = 0;

void generate_keypair() {
  if (keys_generated) return;
  const char *pers = "ecdh";

  mbedtls_ecdh_init(&ecdh_ctx);
  mbedtls_entropy_init(&entropy_ctx);
  mbedtls_ctr_drbg_init(&ctr_drbg_ctx);

  int ret = mbedtls_ctr_drbg_seed(&ctr_drbg_ctx, mbedtls_entropy_func, &entropy_ctx,
                                    (const unsigned char *)pers, strlen(pers));
  if (ret != 0) {
      Serial.printf("mbedtls_ctr_drbg_seed failed: -0x%04X\n", -ret);
      return;
  }

  ret = mbedtls_ecdh_setup(&ecdh_ctx, MBEDTLS_ECP_DP_SECP256R1);
  if (ret != 0) {
      Serial.printf("mbedtls_ecdh_setup failed: -0x%04X\n", -ret);
      return;
  }

  // generates key pair
  ret = mbedtls_ecdh_make_public(&ecdh_ctx, &public_key_size, public_key_buffer, sizeof(public_key_buffer), mbedtls_ctr_drbg_random, &ctr_drbg_ctx);
  if (ret != 0) {
      Serial.printf("mbedtls_ecdh_make_public failed: -0x%04X\n", -ret);
      return;
  }

  Serial.println("Key pair generated.");
  keys_generated = true;
}

void send_public_key(WiFiClient &client) {
  if (!keys_generated) {
      Serial.println("Keys not generated.");
      return;
  }

  client.write((byte *)&public_key_size, sizeof(public_key_size));
  delay(10);
  client.write(public_key_buffer, public_key_size);
  delay(10);
}

void receive_public_key(WiFiClient &client) {
  if (peer_public_key_received) return;

  size_t peer_key_size = 0;
  int bytes_read = client.readBytes((char *)&peer_key_size, sizeof(peer_key_size));
  if (bytes_read != sizeof(peer_key_size)) {
    Serial.println("Failed to read peer key size.");
    return;
  }

  byte peer_key_buf[PUB_KEY_BUFFER_SIZE] = {0};
  bytes_read = client.readBytes((char *)peer_key_buf, peer_key_size);
  if (bytes_read != peer_key_size) {
    Serial.println("Failed to read full peer key.");
    return;
  }

  int ret = mbedtls_ecdh_read_public(&ecdh_ctx, peer_key_buf, peer_key_size);
  if (ret != 0) {
    Serial.printf("mbedtls_ecdh_read_public failed: -0x%04X\n", -ret);
    return;
  }

  peer_public_key_received = true;
}

bool generate_shared_secret() {
  if (!peer_public_key_received) {
    Serial.println("No peer key received.");
    return false;
  }

  size_t secret_len = 0;

  int ret = mbedtls_ecdh_calc_secret(&ecdh_ctx, &secret_len, key, sizeof(key), mbedtls_ctr_drbg_random, &ctr_drbg_ctx);
  if (ret != 0) {
    Serial.printf("mbedtls_ecdh_calc_secret failed: -0x%04X\n", -ret);
    return false;
  }
  return true;
}
