#define WLAN_SSID       
#define WLAN_PASS       
#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <SPI.h>
#include <MFRC522.h>
#include "HX711.h"

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    ""
#define AIO_KEY         ""

const int LOADCELL_DOUT_PIN = D2; 
const int LOADCELL_SCK_PIN = D3;
#define SS_PIN  D10
#define RST_PIN D9

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish feedGlas = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/glas-2");
Adafruit_MQTT_Publish feedGewicht = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/gewicht-2");
Adafruit_MQTT_Publish feedError = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/error2");
Adafruit_MQTT_Publish feedEmpty = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/leer-warnung2");

HX711 scale;
MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- VARIABLEN (Alles in Gramm [g]) ---
float glassWeight = 0.0; 
float maxLiquidWeight = 0.0; 
String lastDrinkName = "";
bool lastEmptyState = false; 
bool errorActive = false;

unsigned long lastReportTime = 0;
const long reportInterval = 4000; 
const int PAGE_ADDR = 4;
const float TOLERANCE = 15.0; // 15g Toleranz gegen Rauschen

void MQTT_connect();
void waitForInput();

void setup() {
  Serial.begin(115200);
  delay(2000);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  SPI.begin();
  mfrc522.PCD_Init();

  // --- KALIBRIERUNG (mit 380g) ---
  Serial.println("--- KALIBRIERUNG START (Einheit: Gramm) ---");
  Serial.println("1. Waage leer lassen. Type '1' + ENTER.");
  waitForInput();
  
  scale.set_scale(); 
  scale.tare(20);
  Serial.println("Tariert.");

  Serial.println("2. Stelle genau 380g auf. Type '1' + ENTER.");
  waitForInput();
  
  long reading = scale.get_value(20); 
  float newFactor = (float)reading / 380.0; // Faktor für Gramm
  scale.set_scale(newFactor);

  Serial.print("Neuer Faktor: "); Serial.println(newFactor);
  Serial.println("System bereit...");
}

void loop() {
  MQTT_connect();

  // --- 1. RFID SCAN (Eher sporadisch / für neue Gläser) ---
  // checken nur, wenn kein Glas bekannt ist oder ein neues erzwingen wollen
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    byte buffer[18];
    byte size = sizeof(buffer);
    if (mfrc522.MIFARE_Read(PAGE_ADDR, buffer, &size) == MFRC522::STATUS_OK) {
      byte drinkID = buffer[0];
      String currentDrink = "";
      float currentGlassW = 0.0;
      float currentMaxL = 0.0;

      // Werte in Gramms
      if (drinkID == 1)      { currentDrink = "Softdrink"; currentGlassW = 380.0; currentMaxL = 300.0; }
      else if (drinkID == 2) { currentDrink = "Wein";      currentGlassW = 150.0; currentMaxL = 200.0; }
      else if (drinkID == 3) { currentDrink = "Bier";      currentGlassW = 400.0; currentMaxL = 500.0; }

      if (currentDrink != "" && currentDrink != lastDrinkName) {
        if (feedGlas.publish(currentDrink.c_str())) {
          lastDrinkName = currentDrink;
          glassWeight = currentGlassW;
          maxLiquidWeight = currentMaxL;
          Serial.print("Glas erkannt: "); Serial.println(lastDrinkName);
          
          if (errorActive) {
            feedError.publish("Kein Fehler");
            errorActive = false;
          }
        }
      }
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  // --- 2. MESSUNG & LOGIK ---
  if (millis() - lastReportTime > reportInterval) {
    lastReportTime = millis();

    float currentRaw = scale.get_units(10); // Gewicht in Gramm
    Serial.print("Waage Brutto: "); Serial.print(currentRaw, 1); Serial.println(" g");

    // PRÄSENZ-LOGIK: 
    // Wenn ein Glas bekannt ist, aber das Gewicht unter 75% des Leergewichts fällt -> Glas entfernt
    if (lastDrinkName != "" && currentRaw < (glassWeight * 0.75)) {
        if (!errorActive) {
          feedError.publish("Glas entfernt");
          feedEmpty.publish("false"); // Lampe aus
          errorActive = true;
          Serial.println("STATUS: Glas wurde weggenommen.");
        }
        lastDrinkName = "";
        glassWeight = 0;
    } 
    // FALLS ein Glas und das Gewicht passt:
    else if (lastDrinkName != "") {
      float liquidWeight = currentRaw - glassWeight;
      
      // Kleiner Puffer gegen Rauschen
      if (liquidWeight < TOLERANCE) liquidWeight = 0.0;

      // Senden
      Serial.print("Netto (Inhalt): "); Serial.print(liquidWeight); Serial.println(" g");
      feedGewicht.publish(liquidWeight);

      // 10% Logik
      bool isEmpty = (liquidWeight < (maxLiquidWeight * 0.10));
      if (isEmpty != lastEmptyState) {
        if (feedEmpty.publish(isEmpty ? "true" : "false")) {
          lastEmptyState = isEmpty;
        }
      }
    } 
    // FALLS kein Glas da ist und kein RFID erkannt wurde -> Error zeigen
    else if (lastDrinkName == "" && !errorActive) {
        // senden den Fehler nur, wenn die Waage auch wirklich leer ist
        if (currentRaw < 50.0) { 
           feedError.publish("Kein Glas erkannt");
           errorActive = true;
        }
    }
  }
}

void waitForInput() {
  while (true) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '1') {
        while(Serial.available()) Serial.read();
        return;
      }
    }
    delay(10);
  }
}

void MQTT_connect() {
  if (mqtt.connected()) return;
  int8_t ret;
  while ((ret = mqtt.connect()) != 0) {
    mqtt.disconnect();
    delay(5000);
  }
}