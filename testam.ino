#include <Arduino.h>
#include <math.h>

// --- CONFIGURARE PINI FIZICI ---
const int PIN_SERVO_MARE     = 18; // Index 0
const int PIN_SERVO_ARATATOR = 23; // Index 1
const int PIN_SERVO_MIJLOCIU = 19; // Index 2
const int PIN_SERVO_INELAR   = 22; // Index 3 -> INVERSAT
const int PIN_SERVO_MIC      = 21; // Index 4 -> INVERSAT

const int piniDegete[] = {PIN_SERVO_MARE, PIN_SERVO_ARATATOR, PIN_SERVO_MIJLOCIU, PIN_SERVO_INELAR, PIN_SERVO_MIC};
const int nrDegete = 5;

#define SENSOR1_PIN 34
#define SENSOR2_PIN 35
#define WINDOW_SIZE 100  
#define STEP_SIZE 50     
#define NUM_VOTES 7      

// --- PARAMETRI PWM 12-BIT ---
const int PWM_FREQ = 50;
const int PWM_RES = 12;
const int POS_0 = 102;  // 0 grade (repaus standard)
const int POS_1 = 512;  // 180 grade (flexie standard)

struct Centroid {
  const char* nume;
  float mav1, rms1, mav2, rms2;
};

const int NUM_GESTURI = 5; 
Centroid centroizi[] = {
  // Nume,       MAV1,             RMS1,             MAV2,             RMS2
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
  analogReadResolution(12);
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  
  for(int i = 0; i < NUM_VOTES; i++) voti[i] = 0; 

  // Initializare PWM 12-bit cu esalonare si inversare pentru pinii 22 si 21 la boot
  for (int i = 0; i < nrDegete; i++) {
    if(ledcAttach(piniDegete[i], PWM_FREQ, PWM_RES)) {
      int pozitieInitiala = POS_0;
      
      // La REPAUS, pinii 22 (index 3) si 21 (index 4) stau deschisi invers (adica POS_1)
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
  
  // Logica nativa a gesturilor
  switch(g) {
    case 0: // ARATATOR 
      pozitii[0]=POS_0; pozitii[1]=POS_1; pozitii[2]=POS_0; pozitii[3]=POS_0; pozitii[4]=POS_0; break;
    case 1: // MIJLOCIU 
      pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_1; pozitii[3]=POS_0; pozitii[4]=POS_0; break;
    case 2: // INELAR 
      pozitii[0]=POS_0; pozitii[1]=POS_0; pozitii[2]=POS_0; pozitii[3]=POS_1; pozitii[4]=POS_0; break;
    case 3: // REPAUS 
      for(int i=0; i<5; i++) pozitii[i] = POS_0; break;
    case 4: // PUMN 
      for(int i=0; i<5; i++) pozitii[i] = POS_1; break;
    default: return;
  }

  // --- INVERSARE LOGICA PINI 22 SI 21 ---
  // Inversam starea doar pentru Inelar (index 3, pin 22) si Mic (index 4, pin 21)
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
      
      executaGest(castigator); 
      
      lastSampleTime = micros(); 
      newSamplesCount = 0;
    }
  }
}