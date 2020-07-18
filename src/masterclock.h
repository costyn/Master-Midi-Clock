#include <Arduino.h>
#include <MIDI.h>
#include <ArduinoTapTempo.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <TaskScheduler.h>
#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <FlexiTimer2.h>


void sendStartStop();
void newBpmSet();
void setTickInterval();
void writeToLcd() ;
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);