#include "HX711.h"

const int LOADCELL_DOUT_PIN = D2;
const int LOADCELL_SCK_PIN = D3;  

HX711 scale;
bool isCalibrated = false; 

void setup() {
  Serial.begin(115200);
  delay(2000); 

  Serial.println("------------------------------------------------");
  Serial.println("Interaktive Waagen-Kalibrierung (Standard Lib)");
  Serial.println("------------------------------------------------");
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  // Rohdaten-Test
  scale.set_scale(); 
  
  Serial.println("1. Waage wird genullt (Tara)... Bitte LEER lassen!");
  delay(1000);
  Serial.print("Test");
  scale.tare(); // Setzt die Waage auf 0
  Serial.print("Test1");
  Serial.println("-> Waage ist genullt.");
  
  Serial.println("\n2. Lege jetzt einen Gegenstand auf (Handy, Wasser, etc.).");
  Serial.println("3. Tippe das Gewicht dieses Gegenstands in KG in die Zeile oben ein.");
  Serial.println("   (Beispiel: Fuer 100g Schokolade tippe: 0.1)");
  Serial.println("   (Beispiel: Fuer 185g Handy tippe: 0.185)");
  Serial.println("4. Druecke ENTER.");
  Serial.println("------------------------------------------------");
}

void loop() {
  // Modus 1: Kalibrierung läuft noch
  if (!isCalibrated) {
    
    // Warten auf Eingabe im Serial Monitor
    if (Serial.available()) {
      float knownWeight = Serial.parseFloat(); // Liest deine Zahl
      
      if (knownWeight != 0) {
        Serial.print("Messe fuer Gewicht: ");
        Serial.print(knownWeight);
        Serial.println(" kg...");
        
        // messen den Durchschnitt von 20 Werten für Genauigkeit
        float measuredVal = scale.get_units(20);
        
        Serial.print("Gemessener Rohwert: ");
        Serial.println(measuredVal);

        // Berechnung des Faktors: Gemessener Rohwert / Echtes Gewicht
        float newFactor = measuredVal / knownWeight;
        
        scale.set_scale(newFactor); // Faktor anwenden
        isCalibrated = true;        // Fertig!
        
        Serial.println("\nERFOLG! Kalibrierung fertig.");
        Serial.print("Dein Kalibrierungs-Faktor ist: ");
        Serial.println(newFactor);
        Serial.println("(Kopiere dir diesen Wert, um ihn spaeter fest im Code zu nutzen!)");
        Serial.println("------------------------------------------------\n");
      }
    }
  } 
  // Modus 2: Waage ist fertig eingestellt
  else {
    // Durchschnitt von 10 Messungen für stabile Anzeige
    float weight = scale.get_units(10);
    
    Serial.print("Aktuelles Gewicht: ");
    Serial.print(weight, 3); // 3 Nachkommastellen
    Serial.println(" kg");
    
    delay(500);
  }
}