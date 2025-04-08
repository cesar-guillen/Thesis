#include <ASCON.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <stdio.h>
#include <string.h>
#include "../ascon-suite-master/apps/asconcrypt/fileops.h"
#include "../ascon-suite-master/apps/asconcrypt/readpass.h"
#include "../ascon-suite-master/apps/asconcrypt/readpass.c"
#include "../ascon-suite-master/apps/asconcrypt/fileops.c"
#include "../ascon-suite-master/apps/asconcrypt/asconcrypt.c"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
#ifdef REASSIGN_PINS
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {
#else
  if (!SD.begin()) {
#endif
    Serial.println("Card Mount Failed");
    return;
  }
  listDir(SD, "/", 0);
  int result = encrypt_file("/zip.zip",&SD, "/garbage");
  hash_file(SD, "/zip.zip");
  //readFile(SD, "/out");
  int result2 = decrypt_file("/garbage",&SD, "/decryptedzip.zip");
  //readFile(SD, "/decrypted.txt");
  Serial.printf("result is %d", result2);
  hash_file(SD,"/decryptedzip.zip");
}

void loop() {
  // put your main code here, to run repeatedly:

}
