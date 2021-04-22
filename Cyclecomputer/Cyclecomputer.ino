#include <Arduino.h>
#include "Button.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <RTClib.h>

Button button_1(4);
Button button_2(7);
Button button_3(8);

const byte interruptPin = 2;

volatile byte rpmcount;
unsigned int rpm;
unsigned long timeold;

double circumference;
double distance;
double speedKm;

unsigned long currenTime = 0;
double timeOfRotation = 0;

double wheelSize; // mm
int personAge; // years
int personWeight; // kg

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27,20,4);
int mode;
unsigned long displayRefreshTime;

bool runR = false;
bool paused = false;
long timer = 0;
long start = 0;
long current = 0;
long previous = 0;
bool once = true;

int arrIndex = 0;
int arrayAverageKmh[100];
int lastAverage = 0;
int currentAverage = 0;

int maxSpeed;

void rpm_fun() {
rpmcount++;
}

//--------------------------------------------------------------------------

void setup() {
  button_1.begin();
  button_2.begin();
  button_3.begin();
  
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(interruptPin), rpm_fun, RISING);
  
  mode = 1;
  maxSpeed = 0;
  
  // Initialize the lcd
  lcd.init();
  displayRefreshTime = 1000; // 1 sec                       
  
  // Print a starting message to the LCD.
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Bike computer");
  lcd.setCursor(0,1);
  lcd.print("Speedometer");
  
  // Store the personal data values on the EEPROM so when the power is cut off they don't get lost
  wheelSize = EEPROM.get(0, wheelSize);
  personAge = EEPROM.get(10, personAge);
  personWeight = EEPROM.get(20, personWeight);
  
  rpmcount = 0;
  rpm = 0;
  timeold = 0;
  
  circumference = wheelSize * 0.001; // meters
  speedKm = 0; // km/h
  distance = 0; //kilometers
  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    
    // Following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    // Following line sets the RTC with an explicit date & time
    // for example to set April 9 2021 at 12:56 you would call:
    // rtc.adjust(DateTime(2021, 4, 9, 12, 56, 00));
  }
}

//--------------------------------------------------------------------------

void loop() {
  calculateSpeed();
  calculateAverageSpeed();
  if(millis() == 3000){
    normalMode(mode);
    dataNormalMode(mode);
  }
  if(millis() > 3000){
    if((millis() - displayRefreshTime) >= 1000){
      displayRefreshTime += 1000;
      if(runR){
        dataSportMode(mode);
      }
      else {
        dataNormalMode(mode);
      }
    } 
    DisplayOutput();
  }
}

//--------------------------------------------------------------------------

void calculateSpeed(){ // calculate current speed & travelled distance
  currenTime = millis();
  if(currenTime-timeold >= 5000){
    speedKm = 0;
  }
  if(rpmcount>=1) {
    timeOfRotation = (currenTime-timeold) * 0.001; // seconds
    speedKm = (circumference/ timeOfRotation) * 3.6; // km/h

    distance += circumference * 0.001; // km
    timeold = currenTime;
    rpmcount = 0;

    if(!paused && runR){
      arrayAverageKmh[arrIndex] = speedKm;
      arrIndex++;

      if(maxSpeed < speedKm){
        maxSpeed = speedKm;
      }  
    }
  }
}

//--------------------------------------------------------------------------

void changeVariables(char symbol){ // update & display the personal data
  switch(symbol){
    case '-':
      switch(mode){
        case 1: // wheel size 
          wheelSize -= 5;
          EEPROM.put(0, wheelSize);
          circumference = wheelSize * 0.001;
          personalData(mode);
          break;
        case 2: // person age
          personAge--;
          EEPROM.put(10, personAge);
          personalData(mode);
          break;
        case 3: // person age
          personWeight--;
          EEPROM.put(20, personWeight);
          personalData(mode);
          break;     
      } 
    break;
    case '+':
      switch(mode){
        case 1: // wheel size 
          wheelSize += 5;
          EEPROM.put(0, wheelSize);
          circumference = wheelSize * 0.001;
          personalData(mode);
          break;
        case 2: // person age
          personAge++;
          EEPROM.put(10, personAge);
          personalData(mode);
          break;
        case 3: // person age
          personWeight++;
          EEPROM.put(20, personWeight);
          personalData(mode);
          break;     
      } 
    break;            
  }
}

//--------------------------------------------------------------------------

void personalData(int stage) { // display the personal data
  lcd.clear();
  switch(stage) {
    case 1: // wheel size 
      lcd.setCursor(0,0);
      lcd.print("Wheel size (mm)");
      lcd.setCursor(0,1);
      lcd.print(wheelSize, 0);
      break;
    case 2: // person age
      lcd.setCursor(0,0);
      lcd.print("Enter age");
      lcd.setCursor(0,1);
      lcd.print(personAge);
      break;  
    case 3: // person weight
      lcd.setCursor(0,0);
      lcd.print("Enter weight");
      lcd.setCursor(0,1);
      lcd.print(personWeight);
      break;  
  }
}

//--------------------------------------------------------------------------

void DisplayOutput(){
  if(!runR){
    if(button_1.isReleased()){ // open the personal data menu
      mode = 1;
      personalData(mode);
      while(mode <= 3){
        if(button_1.isReleased()){ // go to the next page
          mode++;
          personalData(mode);
        }
        if(button_2.isReleased()){ // lower the values
          changeVariables('-');
        }
        if(button_3.isReleased()){ // higher the values
          changeVariables('+');
        }
      }
      mode = 1;
      normalMode(mode);
    }
  }

  if(button_2.isReleased()){ // start & stop the run
    runR = !runR;
  
    paused = false;
    once = true;
    
    previous = 0;
    start = 0;
    timer = 0;
    speedKm = 0;
    lastAverage = 0;
    maxSpeed = 0;
    distance = 0;
    mode = 1;
  
    if(runR){ // update the menu
      sportMode(mode);
      dataSportMode(mode);
    }
    else{
      normalMode(mode);
      dataNormalMode(mode);
    }
  }
  if(runR){
    if(!paused){
      if(once){
        start = millis();
        once = false;
      }
      current = millis();
      timer = previous + (current - start);
    }
    else{
        previous = timer;
    }
    if(button_1.isReleased()){ // pause & start the timer of the run
      paused = !paused;
      once = true;
    }
  }
  
  if(button_3.isReleased()){ // switch between the different modes in the menu
    mode++;
    if(mode == 3){
      mode = 1;
    }
    if(runR){
      sportMode(mode);
      dataSportMode(mode);
    }
    else{
      normalMode(mode);
      dataNormalMode(mode);
    }
  }
}
//--------------------------------------------------------------------------

void normalMode(int stage) { // called once => put the constant string to the display
  lcd.clear();
  switch(stage) {
    case 1:
    lcd.setCursor(0,0);
    lcd.print("Speed ");
    lcd.setCursor(12,0);
    lcd.print("km/h");
    lcd.setCursor(0,1);
    lcd.print("distance");
    lcd.setCursor(14,1);
    lcd.print("km");
    break;
  case 2: 
    lcd.setCursor(0,0);
    lcd.print("Time"); // current time
    lcd.setCursor(0,1);
    lcd.print("Date"); // current date
    break;
  }  
}

//--------------------------------------------------------------------------

void dataNormalMode(int stage) { // update the changing information on the display
  switch(stage) {
    case 1: 
      lcd.setCursor(6,0);
      lcd.print(speedKm, 0); // current speed (km/h)    
      lcd.setCursor(9,1);
      lcd.print(distance); // current distance travelled (km/h)
      break;
   case 2: 
      DateTime now = rtc.now();
      
      lcd.setCursor(5,0);
      lcd.print(now.hour(), DEC);
      lcd.print(':');
      lcd.print(now.minute(), DEC);
      lcd.print(':');
      lcd.print(now.second(), DEC);
      lcd.setCursor(5,1);
      lcd.print(now.day(), DEC);
      lcd.print('/');
      lcd.print(now.month(), DEC);
      lcd.print('/');
      lcd.print(now.year(), DEC);
      break;
  }
}

//--------------------------------------------------------------------------

void sportMode(int stage) { // called once => put the constant string to the display
  lcd.clear();
  switch(stage) {
    case 1: 
      lcd.setCursor(0,0);
      lcd.print("S ");           
      lcd.setCursor(6,0);
      lcd.print("T");
      lcd.setCursor(0,1);
      lcd.print("Travelled ");           
      lcd.setCursor(14,1);
      lcd.print("km");
      break;
    case 2: 
      lcd.setCursor(0,0);
      lcd.print("TOP ");             
      lcd.setCursor(12,0);
      lcd.print("km/h");
      lcd.setCursor(0,1);
      lcd.print("AVG ");         
      lcd.setCursor(12,1);
      lcd.print("km/h");
      break; 
  }
}

//--------------------------------------------------------------------------

void dataSportMode(int stage) { // update the changing information on the display
  switch(stage) {
    case 1: 
      lcd.setCursor(3,0);
      lcd.print(speedKm, 0); // current speed (km/h)            
      lcd.setCursor(8,0);
      lcd.print(timeToString(timer));  
      lcd.setCursor(10,1);
      lcd.print(distance, 1); // current distance travelled (km/h)
      break;
    case 2:
      lcd.setCursor(5,0);
      lcd.print(maxSpeed); // top speed (km/h)             
      lcd.setCursor(5,1);
      lcd.print(lastAverage); // aveage speed (km/h)          
      break;
  }
}

//--------------------------------------------------------------------------

void calculateAverageSpeed() { // calculate average speed in every 100 wheel rotations
  if(arrIndex == 100){
    for(int i = 0; i < arrIndex; i++){
      currentAverage += arrayAverageKmh[i];
    }
    currentAverage /= arrIndex;
    arrIndex = 0;
    if(lastAverage != 0){
      lastAverage = (lastAverage + currentAverage) / 2;
    }
    else {
      lastAverage = currentAverage;
    }
  }
}

//--------------------------------------------------------------------------

String timeToString(long timeNow) { // convert millisecond to time (hours-minutes-seconds)
  
  unsigned long allSeconds = timeNow / 1000;
  int runHours = allSeconds/3600;
  int secsRemaining = allSeconds % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;
  
  char result[21];
  sprintf(result,"%02d:%02d:%02d",runHours,runMinutes,runSeconds);
  String finalResult = result;
  
  return finalResult;
}
