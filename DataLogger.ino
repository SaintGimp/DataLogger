#include <SdFat.h>
#include <Wire.h>
#include "RTClib.h"
#include "LowPower.h"

const float aref_voltage = 3.32;
const int temperaturePin = A2;
const int cardDetectPin = 8;
const int chipSelectPin = 10;
const int lightSensorPin = A1;
const int clockInteruptPin = 2;
const int breakBeamInteruptPin = 3;
const int peripheralPowerPin = 7;
const int statusPin = 9;

const int sleepIntervalInSeconds = 2;

RTC_DS1337 rtc;
SdFat sd;
SdFile logFile;
volatile int breakBeamEvents = 0;
volatile boolean clockEvent = true;

void setup()
{
  //pinMode(statusPin, OUTPUT);
  pinMode(chipSelectPin, OUTPUT);
  pinMode(cardDetectPin, INPUT_PULLUP);
  pinMode(clockInteruptPin, INPUT_PULLUP);
  pinMode(breakBeamInteruptPin, INPUT_PULLUP);
  pinMode(peripheralPowerPin, OUTPUT);
  
  analogReference(EXTERNAL);
  
  // The interrupt for the clock is handled differently below
  attachInterrupt(1, breakBeamWakeUp, FALLING);

  digitalWrite(peripheralPowerPin, LOW);
  delay(100);

  Wire.begin();
  rtc.begin();
}

void loop()
{  
  DateTime now = rtc.now();

  if (breakBeamEvents > 0) {
    writeDataToLog("beam.txt", now);
    breakBeamEvents--;
  }
  if (clockEvent) {
    writeDataToLog("environ.txt", now);
    setClockAlarm(now);
    clockEvent = false;
  } 

  if (breakBeamEvents == 0 && !clockEvent) {
    sleepUntilWoken();
  }
}

void sleepUntilWoken()
{
  // The clock interrupt is set to LOW, not FALLING, so that
  // we never miss being woken up. Because of that, we also
  // need to attach and detach the interrupt here so that we
  // don't spam the interrupt and throw off I2C timings.
  
  turnOffPeripherals();
  attachInterrupt(0, clockWakeUp, LOW);

  // Enter power down state with ADC and BOD module disabled.
  // Wake up when either of the interrupt pins is low.
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 

  detachInterrupt(0);
  turnOnPeripherals();
}

void setClockAlarm(DateTime& now) {
  rtc.clearAlarm1Flag();
  DateTime alarm1time (now.unixtime() + sleepIntervalInSeconds);
  rtc.setAlarm1Time(alarm1time);
}

void writeDataToLog(char* fileName, DateTime& now) {
  int cardInserted = digitalRead(cardDetectPin);
  if (!cardInserted || !sd.begin(chipSelectPin, SPI_HALF_SPEED)) {
    return;
  }
  
  int temperature = round(getTemperature());
  int lightLevel = getLightLevel();
  
  char dataString[50];
  sprintf(dataString, "%02d/%02d/%02d %02d:%02d:%02d\t%d\t%d\n", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second(), temperature, lightLevel);

  if (logFile.open(fileName, O_WRITE | O_CREAT | O_AT_END)) {
    logFile.write(dataString);
    logFile.close();
  }  
}

float getTemperature()
{
  analogRead(temperaturePin);
  delay(10);

  float voltage = analogRead(temperaturePin) * (aref_voltage / 1023.0);
  float temperatureC = (voltage - 0.5) * 100;
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  
  return temperatureF;
}

int getLightLevel()
{
  analogRead(lightSensorPin);
  delay(10);

  // Converting to lux is kind of imprecise without calibrating the
  // device and we only really care about relative levels anyway so
  // just work with the ADC value
  return analogRead(lightSensorPin);
}

void turnOffPeripherals()
{
  delay(100);
  digitalWrite(peripheralPowerPin, HIGH);
}

void turnOnPeripherals()
{
  digitalWrite(peripheralPowerPin, LOW);
  delay(100);
}

void clockWakeUp()
{
  clockEvent = true;
}

void breakBeamWakeUp()
{
  // This is designed for MCP input - physical switches will bounce and cause a lot of events
  breakBeamEvents++;
}

