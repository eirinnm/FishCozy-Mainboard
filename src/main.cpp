#include <Arduino.h>

const long thermistor_pullup = 100000; //should measure this
//values from Vishay's excel file
const long thermistor_r25 = 102000;
const float thermistor_a1 = 0.003354016;
const float thermistor_b1 = 0.0002460382;
const float thermistor_c1 = 3.405377e-6;
const float thermistor_d1 = 1.03424e-7;

class Chamber{
  public:
  uint8_t basepin;
  uint8_t pwmpin;
  uint8_t thermistorpin;
  
  int8_t status; 
  float power; //ranges from -255 to +255
  float temperature; //current temperature
  float setpoint;
  Chamber(uint8_t base_pin, uint8_t pwm_pin, uint8_t thermistor_pin){
    basepin = base_pin;
    pwmpin = pwm_pin;
    thermistorpin = thermistor_pin;
    status = 0;
    temperature = 25;
    setpoint = 28;
  }

  float refreshTemp(){
    analogRead(thermistorpin); //read it once to switch the multiplexer
    int avgADC = 0;
    for(int j=0;j<8;j++){
      avgADC += analogRead(thermistorpin);
    }
    avgADC = avgADC>>3; //average of 8 readings
    //get resistance from voltage divider as a proportion of the thermistor's nominal resistance
    float R = thermistor_pullup / (1024.0 / avgADC - 1) / thermistor_r25;
    float logR = log(R); 
    //float temp = 1/(thermistor_a1 + thermistor_b1*logR + thermistor_c1*logR*logR + thermistor_d1*logR*logR*logR);
    float temp = 1/(thermistor_a1 + thermistor_b1*logR + thermistor_c1*logR*logR*logR);
    temperature = temp-273.15; 
    return temperature;
  }
  void setOutputs(){
    pinMode(basepin, OUTPUT);
    pinMode(pwmpin, OUTPUT);
    power = constrain(power, -255, 255);
    if(power>0){
      //we need to heat
      digitalWrite(basepin, LOW);
      analogWrite(pwmpin, (int)power);
    }else if(power<0){
      //cooling. Max cooling is when PWM is 0
      //so power = -255, pwm = 0
      //power = -64, pwm = 192
      //so pwm = power+255
      int coolPWM = power+255;
      digitalWrite(basepin, HIGH);
      digitalWrite(pwmpin, coolPWM);
    }else{
      digitalWrite(basepin, LOW);
      digitalWrite(pwmpin, LOW);
    }
  }
  void cycleStatus(){ //quick way to switch directions, mainly for testing
    if(status<1){
      status++;
    }else{
      status=-1;
    }
    power = status * 255;
    setOutputs();
  }
};

void setPwmFrequency(int pin, int divisor);

const byte sensorpins[6] = {A1, A2, A3, A4, A7, A6};
// driver 1 = 3, 2
// driver 2 = 5, 4 //problem?
// driver 3 = 6, 7
// driver 4 = 9, 8
// driver 5 = 10, 12
// driver 6 = 11, A0 //note - we seem to have wired this in reverse
const byte pwmpins[6] = {3, 5, 6, 9, 10, 11};
const byte basepins[6] = {2, 4, 7, 8, 12, A0};
//order these based on the physical layout of the chambers
//  1 2 3 Fan
//  4 5 6 Fan
Chamber chambers[6] = { 
  Chamber(basepins[0], pwmpins[0], sensorpins[5]),
  Chamber(basepins[2], pwmpins[2], sensorpins[1]), 
  Chamber(basepins[1], pwmpins[1], sensorpins[0]), //not working
  Chamber(basepins[5], pwmpins[5], sensorpins[3]), //reversed
  Chamber(basepins[3], pwmpins[3], sensorpins[4]),
  Chamber(basepins[4], pwmpins[4], sensorpins[2]),
};

//SerialEvent variables
String inputString = "";        // a string to hold incoming data
boolean stringComplete = false; // whether the string is complete

void setup() {
  Serial.begin(115200);
  inputString.reserve(20);
  for (int i = 0; i < 6; i++){
    setPwmFrequency(chambers[i].pwmpin, 8); //increase PWM speed
    chambers[i].status = 0;
    chambers[i].setOutputs();
  }
}


unsigned long timeLastUpdate = 0;

void loop() {
  if(millis()-timeLastUpdate>250){
    timeLastUpdate=millis();
    for(int i=0;i<6;i++){
      float temp = chambers[i].refreshTemp();
      Serial.print(temp); Serial.print(' '); 
      Serial.print(chambers[i].setpoint); Serial.print(' ');
      Serial.print(chambers[i].power); Serial.print('\t');     
    }
    Serial.println();
  }
  if (stringComplete){
    if (inputString.startsWith("S")){
      inputString.remove(0, 1);
      char command[inputString.length()];
      inputString.toCharArray(command, inputString.length() + 1);
      char *i;
      char *idx = strtok_r(command, " ", &i);
      char *setpoint = strtok_r(NULL, " ", &i);
      if(idx && setpoint){
        //set the chamber to this new temperature
        byte Idx = atoi(idx);
        float Setpoint = atof(setpoint);
        if((0 <= Idx < 6) && (0 <= Setpoint <= 55)){
          chambers[Idx].setpoint = Setpoint;
        }
      }
    }
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
}

void serialEvent() //this is called by the arduino core after each loop
{
  while (Serial.available())
  {
    // get the new byte:
    char inChar = Serial.read();
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar > 0 && inChar <= 127)
    { //filter out gibberish that sometimes happens startup
      if (inChar == '\n')
      {
        stringComplete = true;
        inputString.trim();
      }
      else
      {
        // add it to the inputString:
        inputString += inChar;
      }
    }
  }
}

void setPwmFrequency(int pin, int divisor)
{
  byte mode;
  if (pin == 5 || pin == 6 || pin == 9 || pin == 10)
  {
    switch (divisor)
    {
    case 1:
      mode = 0x01;
      break;
    case 8:
      mode = 0x02;
      break;
    case 64:
      mode = 0x03;
      break;
    case 256:
      mode = 0x04;
      break;
    case 1024:
      mode = 0x05;
      break;
    default:
      return;
    }
    if (pin == 5 || pin == 6)
    {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    }
    else
    {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  }
  else if (pin == 3 || pin == 11)
  {
    switch (divisor)
    {
    case 1:
      mode = 0x01;
      break;
    case 8:
      mode = 0x02;
      break;
    case 32:
      mode = 0x03;
      break;
    case 64:
      mode = 0x04;
      break;
    case 128:
      mode = 0x05;
      break;
    case 256:
      mode = 0x06;
      break;
    case 1024:
      mode = 0x07;
      break;
    default:
      return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}