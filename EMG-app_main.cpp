#include <Arduino.h>
#include <math.h>
#include "BluetoothSerial.h"
#include <Preferences.h>

BluetoothSerial SerialBT;
Preferences prefs;

// Pini Servomotoare
const int PIN_SERVO_MARE     = 18; 
const int PIN_SERVO_ARATATOR = 23; 
const int PIN_SERVO_MIJLOCIU = 19; 
const int PIN_SERVO_INELAR   = 22; 
const int PIN_SERVO_MIC      = 21; 

const int piniDegete[] = {PIN_SERVO_MARE, PIN_SERVO_ARATATOR, PIN_SERVO_MIJLOCIU, PIN_SERVO_INELAR, PIN_SERVO_MIC};
const int nrDegete = 5;

#define SENSOR1_PIN 33
#define WINDOW_SIZE 100  
#define STEP_SIZE 50     
#define NUM_VOTES 7

const int PWM_FREQ = 50;
const int PWM_RES = 12;
const int POS_0 = 102;  
const int POS_1 = 512;  


struct Centroid {
  float mav = 0, rms = 0, wl = 0, zc = 0; 
  bool calibrat = false;
};

const int NUM_GESTURI = 5; 
Centroid centroizi[NUM_GESTURI];
String numeGesturi[] = {"ARATATOR", "MIJLOCIU", "INELAR", "REPAUS", "PUMN"};

bool modCalibrareBruta = false;
bool modRulareNormala = false;

int buffer1[WINDOW_SIZE];
int writeIdx = 0;
int newSamplesCount = 0;
int voti[NUM_VOTES];
int votIdx = 0;
int gestPrecedent = -1;

void incarcaCentroiziDinFlash() {
  prefs.begin("axis", true); 
  Serial.println("\n[Flash] Se incarca centroizii salvati...");
  
  bool totiIncarcati = true;
  for(int i = 0; i < NUM_GESTURI; i++) {
    String cheie = "c_" + String(i);
    if(prefs.isKey(cheie.c_str())) {
      prefs.getBytes(cheie.c_str(), &centroizi[i], sizeof(Centroid));
      centroizi[i].calibrat = true; 
      Serial.printf("-> Incarcat %s: MAV=%.2f, RMS=%.2f, WL=%.2f, ZC=%.2f\n", 
                    numeGesturi[i].c_str(), centroizi[i].mav, centroizi[i].rms, centroizi[i].wl, centroizi[i].zc);
    } else {
      totiIncarcati = false;
    }
  }
  prefs.end();
  
  if(totiIncarcati) {
    Serial.println("[Flash] Toate profilele au fost restaurate cu succes!");
    modRulareNormala = true; 
  } else {
    Serial.println("[Flash] Nu s-au gasit profiluri complete. Necesita calibrare noua.");
  }
}

void salveazaCentroidInFlash(int id) {
  prefs.begin("axis", false); 
  String cheie = "c_" + String(id);
  prefs.putBytes(cheie.c_str(), &centroizi[id], sizeof(Centroid));
  prefs.end();
  Serial.printf("[Flash] Centroidul %d a fost securizat in Flash.\n", id);
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("AXIS"); 
  delay(500);

  incarcaCentroiziDinFlash();

  analogReadResolution(12);
  pinMode(SENSOR1_PIN, INPUT);
  
  for(int i = 0; i < NUM_VOTES; i++) voti[i] = 3; 

  for (int i = 0; i < nrDegete; i++) {
    if(ledcAttach(piniDegete[i], PWM_FREQ, PWM_RES)) {
      ledcWrite(piniDegete[i], (i == 3 || i == 4) ? POS_1 : POS_0);
      delay(50); 
    }
  }
}

void executaGest(int g) {
  if (g == gestPrecedent) return; 
  int pozitii[5];
  
  switch(g) {
    case 0: pozitii[0]=POS_0; pozitii[1]=POS_1; pozitii[2]=POS_0; pozitii[3]=POS_0; pozitii[4]=POS_0; break; 
    case 1: pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_1; pozitii[3]=POS_0; pozitii[4]=POS_0; break; 
    case 2: pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_0; pozitii[3]=POS_1; pozitii[4]=POS_0; break; 
    case 3: for(int i=0; i<5; i++) pozitii[i] = POS_0; break;                                                  
    case 4: for(int i=0; i<5; i++) pozitii[i] = POS_1; break;                                                  
    default: return;
  }

  pozitii[3] = (pozitii[3] == POS_0) ? POS_1 : POS_0;
  pozitii[4] = (pozitii[4] == POS_0) ? POS_1 : POS_0;

  for (int i = 0; i < nrDegete; i++) {
    ledcWrite(piniDegete[i], pozitii[i]);
  }
  gestPrecedent = g;
}

void proceseazaComenziBluetooth() {
  static String inputString = ""; 
  
  while (SerialBT.available()) {
    char inChar = (char)SerialBT.read();
    if (inChar == '\n') {
      inputString.trim();
      
      if (inputString == "CMD:START_RAW") {
        modCalibrareBruta = true;
        modRulareNormala = false;
      }
      else if (inputString == "CMD:STOP_RAW") {
        modCalibrareBruta = false;
      }
      else if (inputString == "CMD:START_RUN") {
        bool totiCalibrati = true;
        for(int i=0; i<NUM_GESTURI; i++) {
          if(!centroizi[i].calibrat) totiCalibrati = false;
        }
        if(totiCalibrati) {
          modRulareNormala = true;
          modCalibrareBruta = false;
          SerialBT.println("STATUS_RUN:OK");
        } else {
          SerialBT.println("ERR:Sistem necalibrat complet in Flash!");
        }
      }
      else if (inputString.startsWith("CENTROID:")) {
        String date = inputString.substring(9);
        int idxComa1 = date.indexOf(',');
        int idxComa2 = date.indexOf(',', idxComa1 + 1);
        int idxComa3 = date.indexOf(',', idxComa2 + 1);
        int idxComa4 = date.indexOf(',', idxComa3 + 1);

        int id = date.substring(0, idxComa1).toInt();
        if(id >= 0 && id < NUM_GESTURI) {
          centroizi[id].mav = date.substring(idxComa1 + 1, idxComa2).toFloat();
          
          if (idxComa3 == -1) {
            // Fallback în caz că aplicația Java trimite doar 2 parametri
            centroizi[id].rms = date.substring(idxComa2 + 1).toFloat();
            centroizi[id].wl = 0;
            centroizi[id].zc = 0;
          } else {
            // Parsare completă pentru 4 parametri (MAV, RMS, WL, ZC)
            centroizi[id].rms = date.substring(idxComa2 + 1, idxComa3).toFloat();
            centroizi[id].wl = date.substring(idxComa3 + 1, idxComa4).toFloat();
            centroizi[id].zc = date.substring(idxComa4 + 1).toFloat();
          }
          
          centroizi[id].calibrat = true;
          salveazaCentroidInFlash(id);
          SerialBT.print("ACK_CALIB:"); SerialBT.println(id);
        }
      }
      
      inputString = ""; 
    } else {
      inputString += inChar;
    }
  }
}

//asta mai poate fi iterat
const float PRAG_DISTANTA = 1500.0; 

void loop() {
  proceseazaComenziBluetooth();

  static unsigned long lastSampleTime = 0;
  if (micros() - lastSampleTime >= 1000) { 
    lastSampleTime = micros();
    int valADC1 = analogRead(SENSOR1_PIN);

    if (modCalibrareBruta) {
      SerialBT.print("RAW:");
      SerialBT.print(valADC1); 
      SerialBT.print(",");
      SerialBT.println(0); 
    }

    buffer1[writeIdx] = valADC1;
    writeIdx = (writeIdx + 1) % WINDOW_SIZE;
    newSamplesCount++;

    if (modRulareNormala && newSamplesCount >= STEP_SIZE) {
      
      
      double sum_dc = 0;
      for (int i = 0; i < WINDOW_SIZE; i++) {
        sum_dc += buffer1[i];
      }
      double mean = sum_dc / WINDOW_SIZE;

      
      double sum_mav = 0, sum_sq = 0, sum_wl = 0;
      int zc_count = 0;
      const double NOISE_THRESHOLD = 20.0; // Prag de zgomot pentru Zero Crossing
      
      double prev_val_ac = buffer1[0] - mean;

      
      for (int i = 0; i < WINDOW_SIZE; i++) {
        double val_ac = buffer1[i] - mean;
        
        sum_mav += abs(val_ac);
        sum_sq += (val_ac * val_ac);
        
        if (i > 0) {
          sum_wl += abs(val_ac - prev_val_ac);
          
          if ((val_ac * prev_val_ac < 0) && (abs(val_ac - prev_val_ac) > NOISE_THRESHOLD)) {
            zc_count++;
          }
        }
        prev_val_ac = val_ac;
      }
      
      float current_mav = sum_mav / WINDOW_SIZE;
      float current_rms = sqrt(sum_sq / WINDOW_SIZE);
      float current_wl = sum_wl / WINDOW_SIZE;
      float current_zc = (float)zc_count;

      int gestCurent = 3; // Repaus default
      float dMin = 1000000.0;
      
      //si astea mai pot fi iterate
      float w_mav = 1.0;
      float w_rms = 1.0;
      float w_wl  = 2.0;   
      float w_zc  = 10.0;  

      for (int i = 0; i < NUM_GESTURI; i++) {
        if(!centroizi[i].calibrat) continue;
        
        float d = sqrt(
            w_mav * pow(current_mav - centroizi[i].mav, 2) + 
            w_rms * pow(current_rms - centroizi[i].rms, 2) +
            w_wl  * pow(current_wl  - centroizi[i].wl,  2) +
            w_zc  * pow(current_zc  - centroizi[i].zc,  2)
        );

        if (d < dMin) { dMin = d; gestCurent = i; }
      }

      if (dMin > PRAG_DISTANTA) { gestCurent = 3; }

      
      voti[votIdx] = gestCurent;
      votIdx = (votIdx + 1) % NUM_VOTES;

      int numaratori[NUM_GESTURI] = {0};
      for (int i = 0; i < NUM_VOTES; i++) numaratori[voti[i]]++;
      
      int castigator = 3;
      int maxVoturi = -1;
      for (int i = 0; i < NUM_GESTURI; i++) {
        if (numaratori[i] > maxVoturi) { maxVoturi = numaratori[i]; castigator = i; }
      }

      
      if (SerialBT.connected()) {
        SerialBT.print("GEST:"); SerialBT.println(numeGesturi[castigator]); 
        SerialBT.print("DATA:");
        SerialBT.print(current_mav); SerialBT.print(",");
        SerialBT.print(current_rms); SerialBT.print(",");
        SerialBT.print(current_wl);  SerialBT.print(",");
        SerialBT.println(current_zc); 
      }
      
      executaGest(castigator); 
      newSamplesCount = 0;
    }
  }
}
