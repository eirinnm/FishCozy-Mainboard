#include <Arduino.h>

const long thermistor_pullup = 100000; //should measure this
//values from Vishay's excel file
const long thermistor_r25 = 104000;
const float thermistor_a1 = 0.003354016;
const float thermistor_b1 = 0.0002460382;
const float thermistor_c1 = 3.405377e-6;
const float thermistor_d1 = 1.03424e-7;
const byte sensorpins[6] = {A1, A2, A3, A4, A5, A6};

void setup() {
    Serial.begin(115200);
}

float get_temp_from_adc(int rawADC){
  //get resistance from voltage divider as a proportion of the thermistor's nominal resistance
  float R = thermistor_pullup/(1024.0/rawADC-1) / thermistor_r25;
  float logR = log(R); 
  //float temp = 1/(thermistor_a1 + thermistor_b1*logR + thermistor_c1*logR*logR + thermistor_d1*logR*logR*logR);
  float temp = 1/(thermistor_a1 + thermistor_b1*logR + thermistor_c1*logR*logR*logR);
  temp = temp-273.15; 
  return temp;
}

void loop() {
    for(int i=0;i<6;i++){
    analogRead(sensorpins[i]); //read it once to switch the multiplexer
    int avgADC = 0;
    for(int j=0;j<8;j++){
      avgADC += analogRead(sensorpins[i]);
    }
    avgADC = avgADC>>3; //average of 8 readings
    float temp = get_temp_from_adc(avgADC);
    Serial.print(temp); Serial.print(' ');
  }
  Serial.println();
  delay(50);        
}