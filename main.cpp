#include <Arduino.h>
#include <math.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

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
  const char* nume;
  float mav1, rms1, mav2, rms2;
};

const int NUM_GESTURI = 5; 
Centroid centroizi[] = {
  {"ARATATOR",   260.7924,         336.038021052632, 50.2962,          101.226042105263},
  {"MIJLOCIU",   229.976170212766, 292.495310283688, 25.2944326241135, 67.2815070921986},
  {"INELAR",     297.49590438247,  376.651545816733, 94.0666135458167, 194.912262948207},
  {"REPAUS",     230.229635443038, 267.967529113924, 39.0774379746836, 85.3229518987342},
  {"PUMN",       455.268602362205, 633.457007874016, 184.420223097113, 332.683799212598}
};

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

  analogReadResolution(12);
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  
  for(int i = 0; i < NUM_VOTES; i++) voti[i] = 0; 

  for (int i = 0; i < nrDegete; i++) {
    if(ledcAttach(piniDegete[i], PWM_FREQ, PWM_RES)) {
      int pozitieInitiala = POS_0;
      if (i == 3 || i == 4) {
        pozitieInitiala = POS_1;
      }
      ledcWrite(piniDegete[i], pozitieInitiala);
      delay(50); 
    }
  }
  Serial.println("Sistem pregatit.");
}

void executaGest(int g) {
  if (g == gestPrecedent) return; 

  int pozitii[5];
  
  switch(g) {
    case 0: 
      pozitii[0]=POS_0; pozitii[1]=POS_1; pozitii[2]=POS_0; pozitii[3]=POS_0; pozitii[4]=POS_0; break;
    case 1: 
      pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_1; pozitii[3]=POS_0; pozitii[4]=POS_0; break;
    case 2: 
      pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_0; pozitii[3]=POS_1; pozitii[4]=POS_0; break;
    case 3: 
      for(int i=0; i<5; i++) pozitii[i] = POS_0; break;
    case 4: 
      for(int i=0; i<5; i++) pozitii[i] = POS_1; break;
    default: return;
  }

  pozitii[3] = (pozitii[3] == POS_0) ? POS_1 : POS_0;
  pozitii[4] = (pozitii[4] == POS_0) ? POS_1 : POS_0;

  for (int i = 0; i < nrDegete; i++) {
    ledcWrite(piniDegete[i], pozitii[i]);
    delay(50); 
  }
  gestPrecedent = g;
}

void loop() {
  static unsigned long lastSampleTime = 0;
  if (micros() - lastSampleTime >= 1000) {
    lastSampleTime = micros();
    buffer1[writeIdx] = analogRead(SENSOR1_PIN);
    buffer2[writeIdx] = analogRead(SENSOR2_PIN);
    writeIdx = (writeIdx + 1) % WINDOW_SIZE;
    newSamplesCount++;

    if (newSamplesCount >= STEP_SIZE) {
      double s1 = 0, s2 = 0, sq1 = 0, sq2 = 0;
      for (int i = 0; i < WINDOW_SIZE; i++) {
        s1 += buffer1[i]; s2 += buffer2[i];
        sq1 += (double)buffer1[i] * buffer1[i];
        sq2 += (double)buffer2[i] * buffer2[i];
      }
      float m1 = s1 / WINDOW_SIZE, m2 = s2 / WINDOW_SIZE;
      float r1 = sqrt(sq1 / WINDOW_SIZE), r2 = sqrt(sq2 / WINDOW_SIZE);

      int gestCurent = 0;
      float dMin = 1000000.0;
      for (int i = 0; i < NUM_GESTURI; i++) {
        float d = sqrt(pow(m1 - centroizi[i].mav1, 2) + pow(r1 - centroizi[i].rms1, 2) + 
                       pow(m2 - centroizi[i].mav2, 2) + pow(r2 - centroizi[i].rms2, 2));
        if (d < dMin) { dMin = d; gestCurent = i; }
      }

      voti[votIdx] = gestCurent;
      votIdx = (votIdx + 1) % NUM_VOTES;

      int numaratori[NUM_GESTURI] = {0};
      for (int i = 0; i < NUM_VOTES; i++) numaratori[voti[i]]++;
      
      int castigator = 0;
      int maxVoturi = -1;
      for (int i = 0; i < NUM_GESTURI; i++) {
        if (numaratori[i] > maxVoturi) {
          maxVoturi = numaratori[i];
          castigator = i;
        }
      }

      Serial.print("Decizie: "); Serial.println(centroizi[castigator].nume);
      
      // Trimite datele prin Bluetooth către aplicație
      if (SerialBT.connected()) {
        // 1. Trimite gestul calculat
        SerialBT.print("GEST:");
        SerialBT.println(centroizi[castigator].nume);
        
        // 2. Trimite MAV și RMS pentru ambii senzori separati prin virgula
        SerialBT.print("DATA:");
        SerialBT.print(m1); SerialBT.print(",");
        SerialBT.print(r1); SerialBT.print(",");
        SerialBT.print(m2); SerialBT.print(",");
        SerialBT.println(r2);
      }
      
      executaGest(castigator); 
      
      lastSampleTime = micros(); 
      newSamplesCount = 0;
    }
  }
}
