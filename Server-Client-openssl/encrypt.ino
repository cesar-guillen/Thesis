#define CHUNK_SIZE 512

void decrypt_verify(String file){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  deleteFile(SD, requested_file_name.c_str());
  Serial.println("Decrypting file ...");
  int result = decrypt_file((requested_file_name + ".ascon").c_str(),&SD,requested_file_name.c_str());
  deleteFile(SD, (requested_file_name + ".ascon").c_str());
  char unsigned  hash_results[CRYPTO_BYTES] = { 0 };
  hash_file(SD, requested_file_name.c_str(), hash_results);

  if(!compare_hashes(hash, hash_results)){
    Serial.println("The file recieved is not correct. Deleting file...");
    deleteFile(SD, requested_file_name.c_str());
  }
  else Serial.println("Hashes match! Download was succesful.");
  listDir(SD, "/", 1);
}

void prepare_file(String file_name){
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  if (check_file(SD,file_name.c_str()) < 0) {
    Serial.println("File is not found. Exiting");
    return;
  }
  String encrypted_filed_name = file_name + ".ascon";
  Serial.printf("Encrypting file %s ...\n", file_name);
  deleteFile(SD, encrypted_filed_name.c_str());
  encrypt_file(file_name.c_str(), &SD, encrypted_filed_name.c_str());
  send_hash(SD, file_name.c_str());
  send_file(SD, encrypted_filed_name.c_str(), file_name.c_str());
  deleteFile(SD, encrypted_filed_name.c_str());
}


int encrypt_message(char* plaintext, char* cyphertext, size_t *clen, size_t mlen, unsigned char *npub){
  unsigned char *m = (unsigned char*)plaintext;
  unsigned char ad[] = "";
  unsigned long long adlen = strlen((char*)ad);

  ascon128_aead_encrypt((unsigned char*)cyphertext, clen, m, mlen, ad, adlen, (const char unsigned*)npub, k);
  return 0;
}

long long unsigned int decrypt_message(char *ciphertext,size_t clen ,char *plaintext, size_t* decrypted_mlen, const unsigned char* npub){
  unsigned char ad[] = "";  // Optional associated data
  unsigned long long adlen = strlen((char*)ad);
  int decrypt_status = ascon128_aead_decrypt((unsigned char*)plaintext, decrypted_mlen, (const unsigned char*)ciphertext, clen, ad, adlen, npub, k);
  if (decrypt_status < 0) {
    Serial.println("Decryption failed!");
    return -1;
  }
  return *decrypted_mlen;
}

void print_hex(const unsigned char* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char buf[3];
    sprintf(buf, "%02X", data[i]);
    Serial.printf("%s ",buf);
  }
  Serial.println();
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

void handleErrors(void);

int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
  unsigned char *iv, unsigned char *ciphertext);

int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
  unsigned char *iv, unsigned char *plaintext);

int main(int argc, char *argv[]) {
  // Initialize OpenSSL
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();

  // Generate a random key and IV
  unsigned char key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];

  if(!RAND_bytes(key, 16) || !RAND_bytes(iv, 16)) {
    handleErrors();
  }

  // Open the input file
  FILE* infile = fopen(argv[1], "rb");
  if (!infile) {
    printf("Failed to open input file %s\n", argv[1]);
    return 1;
  }

  // Calculate the input file size
  fseek(infile, 0, SEEK_END);
  int filesize = ftell(infile);
  fseek(infile, 0, SEEK_SET);

  // Allocate a buffer for the input file data
  unsigned char* plaintext = (unsigned char*) malloc(filesize + 1);

  // Read the input file data
  fread(plaintext, 1, filesize, infile);

  // Encrypt the plaintext
  int ciphertext_len = encrypt(plaintext, filesize, key, iv, plaintext);

  // Open the output file
  FILE* outfile = fopen(argv[2], "wb");
  if (!outfile) {
    printf("Failed to open output file %s\n", argv[2]);
    return 1;
  }

  // Write the ciphertext to the output file
  fwrite(plaintext, 1, ciphertext_len, outfile);

  // Free memory
  fclose(infile);
  fclose(outfile);
  free(plaintext);
  EVP_cleanup();
  ERR_free_strings();

  return 0;
}

void handleErrors(void) {
  fprintf(stderr, "Error encrypting/decrypting data\n");
  ERR_print_errors_fp(stderr);
  abort();
}

int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
  unsigned char *iv, unsigned char *ciphertext) {
  EVP_CIPHER_CTX *ctx;

  int len;
  int ciphertext_len;

  if(!(ctx = EVP_CIPHER_CTX_new())) {
    handleErrors();
  }

  // ... rest of the function

  return ciphertext_len;
}
