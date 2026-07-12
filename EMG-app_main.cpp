#include <Arduino.h>
#include <math.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

//servomotoare
const int PIN_SERVO_MARE     = 18; 
const int PIN_SERVO_ARATATOR = 23; 
const int PIN_SERVO_MIJLOCIU = 19; 
const int PIN_SERVO_INELAR   = 22; 
const int PIN_SERVO_MIC      = 21; 

const int piniDegete[] = {PIN_SERVO_MARE, PIN_SERVO_ARATATOR, PIN_SERVO_MIJLOCIU, PIN_SERVO_INELAR, PIN_SERVO_MIC};
const int nrDegete = 5;

#define SENSOR1_PIN 34
#define SENSOR2_PIN 35
#define WINDOW_SIZE 100  
#define STEP_SIZE 50     
#define NUM_VOTES 7      

const int PWM_FREQ = 50;
const int PWM_RES = 12;
const int POS_0 = 102;  
const int POS_1 = 512;  

struct Centroid {
  String nume;
  float mav1 = 0, rms1 = 0, mav2 = 0, rms2 = 0;
  bool calibrat = false;
};

const int NUM_GESTURI = 5; 
Centroid centroizi[NUM_GESTURI];
String numeGesturi[] = {"ARATATOR", "MIJLOCIU", "INELAR", "REPAUS", "PUMN"};

bool modCalibrareBruta = false;
bool modRulareNormala = false;

int buffer1[WINDOW_SIZE];
int buffer2[WINDOW_SIZE];
int writeIdx = 0;
int newSamplesCount = 0;
int voti[NUM_VOTES];
int votIdx = 0;
int gestPrecedent = -1;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("NeuroGrip_ESP32"); 

  for(int i = 0; i < NUM_GESTURI; i++) {
    centroizi[i].nume = numeGesturi[i];
  }

  analogReadResolution(12);
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  
  for(int i = 0; i < NUM_VOTES; i++) voti[i] = 3; // default REPAUS

  for (int i = 0; i < nrDegete; i++) {
    if(ledcAttach(piniDegete[i], PWM_FREQ, PWM_RES)) {
      ledcWrite(piniDegete[i], (i == 3 || i == 4) ? POS_1 : POS_0);
      delay(50); 
    }
  }
  Serial.println("[NeuroGrip] Sistem pregatit. Asteapta comenzi din aplicatie.");
}

void executaGest(int g) {
  if (g == gestPrecedent) return; 
  int pozitii[5];
  
  switch(g) {
    case 0: pozitii[0]=POS_0; pozitii[1]=POS_1; pozitii[2]=POS_0; pozitii[3]=POS_0; pozitii[4]=POS_0; break; // ARATATOR
    case 1: pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_1; pozitii[3]=POS_0; pozitii[4]=POS_0; break; // MIJLOCIU
    case 2: pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_0; pozitii[3]=POS_1; pozitii[4]=POS_0; break; // INELAR
    case 3: for(int i=0; i<5; i++) pozitii[i] = POS_0; break;                                                 // REPAUS
    case 4: for(int i=0; i<5; i++) pozitii[i] = POS_1; break;                                                 // PUMN
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
  if (SerialBT.available()) {
    String msg = SerialBT.readStringUntil('\n');
    msg.trim();

    if (msg == "CMD:START_RAW") {
      modCalibrareBruta = true;
      modRulareNormala = false;
      Serial.println("Aplicatia a pornit esantionarea bruta.");
    }
    else if (msg == "CMD:STOP_RAW") {
      modCalibrareBruta = false;
      Serial.println("Aplicatia a oprit esantionarea bruta.");
    }
    else if (msg == "CMD:START_RUN") {
      // verif
      bool totiCalibrati = true;
      for(int i=0; i<NUM_GESTURI; i++) {
        if(!centroizi[i].calibrat) totiCalibrati = false;
      }
      if(totiCalibrati) {
        modRulareNormala = true;
        modCalibrareBruta = false;
        Serial.println("Sistemul a intrat in executie bionica stabila.");
      } else {
        SerialBT.println("ERR:Sistem necalibrat complet!");
      }
    }
    else if (msg.startsWith("CENTROID:")) {
      
      String date = msg.substring(9);
      int idxComa1 = date.indexOf(',');
      int idxComa2 = date.indexOf(',', idxComa1 + 1);
      int idxComa3 = date.indexOf(',', idxComa2 + 1);
      int idxComa4 = date.indexOf(',', idxComa3 + 1);

      int id = date.substring(0, idxComa1).toInt();
      if(id >= 0 && id < NUM_GESTURI) {
        centroizi[id].mav1 = date.substring(idxComa1 + 1, idxComa2).toFloat();
        centroizi[id].rms1 = date.substring(idxComa2 + 1, idxComa3).toFloat();
        centroizi[id].mav2 = date.substring(idxComa3 + 1, idxComa4).toFloat();
        centroizi[id].rms2 = date.substring(idxComa4 + 1).toFloat();
        centroizi[id].calibrat = true;
        
        Serial.print("Centroid salvat local pentru gestul ");
        Serial.println(centroizi[id].nume);
        SerialBT.print("ACK_CALIB:"); SerialBT.println(id);
      }
    }
  }
}

void loop() {
  proceseazaComenziBluetooth();

  static unsigned long lastSampleTime = 0;
  if (micros() - lastSampleTime >= 1000) {
    lastSampleTime = micros();
    int valADC1 = analogRead(SENSOR1_PIN);
    int valADC2 = analogRead(SENSOR2_PIN);

    //csvul
    if (modCalibrareBruta) {
      SerialBT.print("RAW:");
      SerialBT.print(valADC1);
      SerialBT.print(",");
      SerialBT.println(valADC2);
    }

    // colectarea in buffer
    buffer1[writeIdx] = valADC1;
    buffer2[writeIdx] = valADC2;
    writeIdx = (writeIdx + 1) % WINDOW_SIZE;
    newSamplesCount++;

    //distanta euclidiana
    if (modRulareNormala && newSamplesCount >= STEP_SIZE) {
      double s1 = 0, s2 = 0, sq1 = 0, sq2 = 0;
      for (int i = 0; i < WINDOW_SIZE; i++) {
        s1 += buffer1[i]; s2 += buffer2[i];
        sq1 += (double)buffer1[i] * buffer1[i];
        sq2 += (double)buffer2[i] * buffer2[i];
      }
      float m1 = s1 / WINDOW_SIZE, m2 = s2 / WINDOW_SIZE;
      float r1 = sqrt(sq1 / WINDOW_SIZE), r2 = sqrt(sq2 / WINDOW_SIZE);

      int gestCurent = 3; // Default REPAUS dacă apar anomalii
      float dMin = 1000000.0;
      
      for (int i = 0; i < NUM_GESTURI; i++) {
        if(!centroizi[i].calibrat) continue;
        float d = sqrt(pow(m1 - centroizi[i].mav1, 2) + pow(r1 - centroizi[i].rms1, 2) + 
                       pow(m2 - centroizi[i].mav2, 2) + pow(r2 - centroizi[i].rms2, 2));
        if (d < dMin) { dMin = d; gestCurent = i; }
      }

      voti[votIdx] = gestCurent;
      votIdx = (votIdx + 1) % NUM_VOTES;

      int numaratori[NUM_GESTURI] = {0};
      for (int i = 0; i < NUM_VOTES; i++) numaratori[voti[i]]++;
      
      int castigator = 3;
      int maxVoturi = -1;
      for (int i = 0; i < NUM_GESTURI; i++) {
        if (numaratori[i] > maxVoturi) {
          maxVoturi = numaratori[i];
          castigator = i;
        }
      }

      if (SerialBT.connected()) {
        SerialBT.print("GEST:"); SerialBT.println(centroizi[castigator].nume);
        SerialBT.print("DATA:");
        SerialBT.print(m1); SerialBT.print(",");
        SerialBT.print(r1); SerialBT.print(",");
        SerialBT.print(m2); SerialBT.print(",");
        SerialBT.println(r2);
      }
      
      executaGest(castigator); 
      newSamplesCount = 0;
    }
  }
}
