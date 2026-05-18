#include <Arduino.h>

// Definire pini ESP32 
#define SENSOR1_PIN 34
#define SENSOR2_PIN 35

// Configurare fereastră (100ms la 1000Hz)
#define WINDOW_SIZE 100  
#define STEP_SIZE 50     // Overlap 50% (calcul la fiecare 50ms)

// Corecție: Declarare ca vectori (arrays)
int buffer1[200];
int buffer2[200];
int writeIdx = 0;
int newSamplesCount = 0;

void setup() {
  Serial.begin(115200);
  
  // ESP32 ADC are 12 biți (0-4095) 
  analogReadResolution(12);
  
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);

  for(int i = 0; i < WINDOW_SIZE; i++) {
    buffer1[i] = 0;
    buffer2[i] = 0;
  }
}

void loop() {
  static unsigned long lastSampleTime = 0;
  
  if (micros() - lastSampleTime >= 1000) {
    lastSampleTime = micros();
    
    // Citire semnal unipolar (deja rectificat intern de TL084) 
    buffer1[writeIdx] = analogRead(SENSOR1_PIN);
    buffer2[writeIdx] = analogRead(SENSOR2_PIN);
    
    writeIdx = (writeIdx + 1) % WINDOW_SIZE;
    newSamplesCount++;
    
    if (newSamplesCount >= STEP_SIZE) {
      // Calcul caracteristici pe fereastra de 100ms
      double sum1 = 0, sum2 = 0;
      double sumSq1 = 0, sumSq2 = 0;
      
      for (int i = 0; i < WINDOW_SIZE; i++) {
        double val1 = (double)buffer1[i];
        double val2 = (double)buffer2[i];
        
        sum1 += val1;
        sum2 += val2;
        sumSq1 += val1 * val1;
        sumSq2 += val2 * val2;
      }
      
      // MAV și RMS
      float mav1 = (float)(sum1 / WINDOW_SIZE);
      float mav2 = (float)(sum2 / WINDOW_SIZE);
      float rms1 = sqrt(sumSq1 / WINDOW_SIZE);
      float rms2 = sqrt(sumSq2 / WINDOW_SIZE);
      
      // Output CSV pentru grafic și fișier text (MAV1,RMS1,MAV2,RMS2)
      Serial.print(mav1); Serial.print(",");
      Serial.print(rms1); Serial.print(",");
      Serial.print(mav2); Serial.print(",");
      Serial.println(rms2);
      
      newSamplesCount = 0;
    }
  }
}
