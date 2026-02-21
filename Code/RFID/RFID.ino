#include <SPI.h>
#include <MFRC522.h>

// Pins für Arduino Nano ESP32
#define SS_PIN  D10
#define RST_PIN D9

// MISO D12
// MOSI D11
// RST D13

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Variable für Schreib-Modus
int writeMode = 0; // 0=Lesen, 1=Softdrink, 2=Wein, 3=Bier

// Page 4 ist der erste sichere Speicherplatz für User-Daten auf NTAG/Ultralight
const int PAGE_ADDR = 4; 

void setup() {
  Serial.begin(115200);
  delay(2000); 

  SPI.begin();        
  mfrc522.PCD_Init(); 

  Serial.println("------------------------------------------------");
  Serial.println("   NFC CHIP WRITER - GETRAENKE-SYSTEM");
  Serial.println("------------------------------------------------");
  Serial.println("BEFEHLE für den Schreibmodus:");
  Serial.println("'1' + Enter -> SCHREIBE: Softdrink (380g Glas)");
  Serial.println("'2' + Enter -> SCHREIBE: Wein      (150g Glas)");
  Serial.println("'3' + Enter -> SCHREIBE: Bier      (400g Glas)");
  Serial.println("'0' + Enter -> NUR LESEN");
  Serial.println("------------------------------------------------");
}

void loop() {
  // 1. Serielle Befehle verarbeiten
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == '1') {
      writeMode = 1;
      Serial.println("\n[MODUS] Nächster Chip wird: SOFTDRINK");
    } else if (command == '2') {
      writeMode = 2;
      Serial.println("\n[MODUS] Nächster Chip wird: WEIN");
    } else if (command == '3') {
      writeMode = 3;
      Serial.println("\n[MODUS] Nächster Chip wird: BIER");
    } else if (command == '0') {
      writeMode = 0;
      Serial.println("\n[MODUS] Nur Lesen.");
    }
    // Buffer leeren (Enter abfangen)
    while(Serial.available()) Serial.read(); 
  }

  // 2. Prüfen ob ein Chip da ist
  if ( ! mfrc522.PICC_IsNewCardPresent()) return;
  if ( ! mfrc522.PICC_ReadCardSerial()) return;

  // --- Chip Aktion ---
  if (writeMode > 0) {
    writeDrinkToNTAG(writeMode);
    writeMode = 0; // Nach dem Schreiben zurück in Lesemodus
    Serial.println("System bereit zum Lesen.");
  } else {
    readDrinkFromNTAG();
  }

  // Karte stoppen
  mfrc522.PICC_HaltA(); 
  mfrc522.PCD_StopCrypto1();
  
  delay(1500); // Kurze Pause zur Stabilisierung
}

void readDrinkFromNTAG() {
  byte buffer[18];
  byte size = sizeof(buffer);

  MFRC522::StatusCode status = mfrc522.MIFARE_Read(PAGE_ADDR, buffer, &size);
  
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Fehler beim Lesen: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  byte drinkID = buffer[0]; 
  Serial.print("\n>>> Chip erkannt! ");
  
  switch(drinkID) {
    case 1:  Serial.println("Inhalt: SOFTDRINK (380g)"); break;
    case 2:  Serial.println("Inhalt: WEIN (150g)"); break;
    case 3:  Serial.println("Inhalt: BIER (400g)"); break;
    default: 
      Serial.print("Unbekannter Inhalt (ID: "); 
      Serial.print(drinkID); 
      Serial.println(")"); 
      break;
  }
}

void writeDrinkToNTAG(int drinkType) {
  byte data[4]; 
  data[0] = (byte)drinkType; // Die ID (1, 2 oder 3)
  data[1] = 0;
  data[2] = 0;
  data[3] = 0;

  Serial.print("Schreibe Daten auf Page 4... ");

  MFRC522::StatusCode status = mfrc522.MIFARE_Ultralight_Write(PAGE_ADDR, data, 4);

  if (status != MFRC522::STATUS_OK) {
    Serial.print("FEHLGESCHLAGEN: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
  } else {
    Serial.println("ERFOLG!");
    Serial.print("Chip ist nun als ");
    if(drinkType == 1) Serial.println("SOFTDRINK markiert.");
    if(drinkType == 2) Serial.println("WEIN markiert.");
    if(drinkType == 3) Serial.println("BIER markiert.");
  }
}