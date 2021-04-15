#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);
unsigned long displayRefreshTime;
#include <RTClib.h>
#include "Button.h"

// Constants won't be changed.
// They're used here to set pin numbers:

Button button_1(4);
Button button_2(7);
Button button_3(8);
Button button_4(12);

bool button1_Pressed = false;
bool button2_Pressed = false;
bool button3_Pressed = false;
bool button4_Pressed = false;

const byte interruptPin = 2;

volatile byte rpmcount;
unsigned int rpm;
unsigned long timeold;

double circumference;
double totalDistance;
double speedKm;

unsigned long currenTime = 0;
double timeOfRotation = 0;

double wheelSize;    // Mm
int personAge;    // Years
int personWeight; // Kg

int stage;
int mode;
int lastButtonPressed;
bool bContinue;

RTC_DS3231 rtc;

bool runR = false;
bool paused = false;
long timer = 0;
long start = 0;
long current = 0;
long previous = 0;
bool once = true;

int arrIndex = 0;
int arrayMaximumKPH[100];
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
  button_4.begin();

  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(interruptPin), rpm_fun, RISING);

  stage = 1;
  mode = 1;
  bContinue = false;
  // lastButtonPressed = ' ';

  maxSpeed = 0;

  //initialize the lcd
  lcd.init();
  displayRefreshTime = 1000; // 0.01 sec

  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Bike computer");
  lcd.setCursor(0, 1);
  lcd.print("Speedo !!!");



  // Store those values on the EEPROM so when the power is cut off they don't get lost
  if (EEPROM.read(0 != 2155)) {
    wheelSize = EEPROM.get(0, wheelSize);
  } else {
    wheelSize = 2155;
    EEPROM.put(0, wheelSize);
  }

  if (EEPROM.read(10 != 35)) {
    personAge = EEPROM.get(10, personAge);
  } else {
    personAge = 35;
    EEPROM.put(10, personAge);
  }

  if (EEPROM.read(20 != 70)) {
    personWeight = EEPROM.get(20, personWeight);
  } else {
    personWeight = 70;
    EEPROM.put(20, personWeight);
  }

  rpmcount = 0;
  rpm = 0;
  timeold = 0;

  //circumference = wheelSize * 0.001; meters
  speedKm = 0; // km/h
  totalDistance = 0; //kilometers

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");

    // Following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Following line sets the RTC with an explicit date & time
    // for example to set January 27 2017 at 12:56 you would call:
    // rtc.adjust(DateTime(2021, 4, 9, 21, 25, 30));
  }
  circumference = wheelSize * 0.001;
}

//--------------------------------------------------------------------------

void loop() {
  calculateSpeed();
  calculateAverageSpeed();
  if (millis() > 3000) {
    if ((millis() - displayRefreshTime) >= 1000) {
      displayRefreshTime += 1000;
      if (runR) {
        sportMode(mode);
      }
      if (!runR) {
        normalMode(mode);
      }
    }
    DisplayOutput();
  }
}

//--------------------------------------------------------------------------

void calculateSpeed() {
  currenTime = millis();
  if (currenTime - timeold >= 5000) {
    speedKm = 0;
  }
  if (rpmcount >= 1) {

    //rpm=(60000)/(currenTime-timeold);// Revolution Per Minute

    timeOfRotation = (currenTime - timeold) * 0.001; // Seconds
    speedKm = (circumference / timeOfRotation) * 3.6; // Km/h

    totalDistance += circumference * 0.001; // Km
    timeold = currenTime;
    rpmcount = 0;

    if (!paused && runR) {
      arrayMaximumKPH[arrIndex] = speedKm;
      arrIndex++;

      if (maxSpeed < speedKm) {
        maxSpeed = speedKm;
      }
    }
  }
}

//--------------------------------------------------------------------------

void changeVariables(char symbol) {
  switch (symbol) {
    case '-':
      switch (stage) {
        case 1: // Wheel size
          wheelSize -= 5;
          circumference = wheelSize * 0.001;
          personalData(stage);
          break;
        case 2: // Person age
          personAge--;
          personalData(stage);
          break;
        case 3: // Person age
          personWeight--;
          personalData(stage);
          break;
      }
      break;
    case '+':
      switch (stage) {
        case 1: // Wheel size
          wheelSize += 5;
          circumference = wheelSize * 0.001;
          personalData(stage);
          break;
        case 2: // Person age
          personAge++;
          personalData(stage);
          break;
        case 3: // Person age
          personWeight++;
          personalData(stage);
          break;
      }
      break;
  }
}

//--------------------------------------------------------------------------

void personalData(int stage) {
  lcd.clear();
  switch (stage) {
    case 1: // Wheel size
      lcd.setCursor(0, 0);
      lcd.print("Wheel size (mm)");
      lcd.setCursor(0, 1);
      lcd.print(wheelSize, 0);
      EEPROM.put(0, wheelSize);
      break;
    case 2: // Person age
      lcd.setCursor(0, 0);
      lcd.print("Enter age");
      lcd.setCursor(0, 1);
      lcd.print(personAge);
      EEPROM.put(10, personAge);
      break;
    case 3: // Person weight
      lcd.setCursor(0, 0);
      lcd.print("Enter weight");
      lcd.setCursor(0, 1);
      lcd.print(personWeight);
      EEPROM.put(20, personWeight);
      break;
  }
}

//--------------------------------------------------------------------------

void DisplayOutput() {
  if (!runR) {
    if (button_1.isReleased()) {
      personalData(stage);
      while (stage <= 3) {
        if (button_1.isReleased()) {
          stage++;
          personalData(stage);
        }
        if (button_2.isReleased()) {
          changeVariables('-');
        }
        if (button_3.isReleased()) {
          changeVariables('+');
        }
      }
      stage = 1;
    }
  }

  if (button_2.isReleased()) {
    runR = !runR;

    paused = false;
    once = true;

    previous = 0;
    start = 0;
    timer = 0;
    speedKm = 0;
    lastAverage = 0;
    maxSpeed = 0;
    totalDistance = 0;
  }
  if (runR) {
    if (!paused) {
      if (once) {
        start = millis();
        once = false;
      }
      current = millis();
      timer = previous + (current - start);
    }
    else {
      previous = timer;
    }
    if (button_1.isReleased()) {
      paused = !paused;
      once = true;
    }
    // Button 3 could be removed
    if (button_3.isReleased()) {
      mode++;
      if (mode == 3) {
        mode = 1;
      }
    }
  }

  if (button_3.isReleased()) {
    mode++;
    if (mode == 3) {
      mode = 1;
    }
  }
}
//--------------------------------------------------------------------------

void normalMode(int stage) {
  lcd.clear();
  switch (stage) {
    case 1: // Speed & distance
      lcd.setCursor(0, 0);
      lcd.print("Speed ");
      lcd.setCursor(6, 0);
      lcd.print(speedKm, 0);  // Km/h
      lcd.setCursor(12, 0);
      lcd.print("km/h");
      lcd.setCursor(0, 1);
      lcd.print("distance");
      lcd.setCursor(9, 1);
      lcd.print(totalDistance);   // Distance travelled
      lcd.setCursor(14, 1);
      lcd.print("km");
      break;
    case 2: // time & date
      DateTime now = rtc.now();

      lcd.setCursor(0, 0);
      lcd.print("Time");
      lcd.setCursor(5, 0);
      lcd.print(now.hour(), DEC);
      lcd.print(':');
      lcd.print(now.minute(), DEC);
      lcd.print(':');
      lcd.print(now.second(), DEC);
      lcd.setCursor(0, 1);
      lcd.print("Date");
      lcd.setCursor(5, 1);
      lcd.print(now.day(), DEC);
      lcd.print('/');
      lcd.print(now.month(), DEC);
      lcd.print('/');
      lcd.print(now.year(), DEC);
      break;
  }
}

//--------------------------------------------------------------------------

void sportMode(int stage) {
  lcd.clear();
  switch (stage) {
    case 1: // Speed, distance & time of the run
      lcd.setCursor(0, 0);
      lcd.print("S ");
      lcd.setCursor(3, 0);
      lcd.print(speedKm, 0);
      lcd.setCursor(6, 0);
      lcd.print("T");
      lcd.setCursor(8, 0);
      lcd.print(timeToString(timer));
      lcd.setCursor(0, 1);
      lcd.print("Travelled ");
      lcd.setCursor(10, 1);
      lcd.print(totalDistance, 1);
      lcd.setCursor(14, 1);
      lcd.print("km");
      break;
    case 2: // Top & average speed
      lcd.setCursor(0, 0);
      lcd.print("TOP ");
      lcd.setCursor(5, 0);
      lcd.print(maxSpeed);              // Top speed (km/h)
      lcd.setCursor(12, 0);
      lcd.print("km/h");
      lcd.setCursor(0, 1);
      lcd.print("AVG ");
      lcd.setCursor(5, 1);
      lcd.print(lastAverage);           // Average speed (km/h)
      lcd.setCursor(12, 1);
      lcd.print("km/h");
      break;

  }
}

//--------------------------------------------------------------------------

void calculateAverageSpeed() {
  if (arrIndex == 100) {
    for (int i = 0; i < arrIndex; i++) {
      currentAverage += arrayMaximumKPH[i];
    }
    currentAverage /= arrIndex;
    arrIndex = 0;
    if (lastAverage != 0) {
      lastAverage = (lastAverage + currentAverage) / 2;
    }
    else {
      lastAverage = currentAverage;
    }
  }
}

//--------------------------------------------------------------------------

String timeToString(long timeNow) {

  unsigned long allSeconds = timeNow / 1000;
  int runHours = allSeconds / 3600;
  int secsRemaining = allSeconds % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;

  char result[21];
  sprintf(result, "%02d:%02d:%02d", runHours, runMinutes, runSeconds);
  String finalResult = result;

  return finalResult;
}
