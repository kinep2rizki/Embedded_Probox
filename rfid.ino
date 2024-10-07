#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 5    // SDA pin
#define RST_PIN 22 // Reset pin

MFRC522 rfid(SS_PIN, RST_PIN);  // Create instance of the MFRC522 class

void setup() {
  Serial.begin(115200);  // Initialize serial communication
  SPI.begin();           // Initialize SPI bus
  rfid.PCD_Init();       // Initialize MFRC522 reader
  
  Serial.println("Place your RFID card near the reader...");
}

void loop() {
  // Look for a new card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;  // No card is present
  }

  // Read the card's UID
  Serial.print("Card UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Halt PICC and stop encryption on the PCD
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
