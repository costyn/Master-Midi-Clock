#define _TASK_MICRO_RES
// #define _TASK_TIMECRITICAL
// #define SOFTSERIAL

#include <MIDI.h>
#include <ArduinoTapTempo.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <TaskScheduler.h>
#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <FlexiTimer2.h>

#ifdef SOFTSERIAL
#include <SoftwareSerial.h>
#endif

#define MIN_BPM 30
#define MAX_BPM 180

#define LED_PLAYING_PIN 6
#define LED_TEMPO_PIN 7
#define BUTTON_STARTSTOP_PIN 2
#define BUTTON_TAP_TEMPO 4

#define PIN_TEMPO_POT 1

#define TASK_CHECK_BUTTON_PRESS_INTERVAL    100000L // 150 milliseconds
#define TASK_CHECK_POT_INTERVAL    100000L // 150 milliseconds

#ifdef SOFTSERIAL
SoftwareSerial SoftSerial(8,9);
MIDI_CREATE_INSTANCE(SoftwareSerial, SoftSerial, MIDI);
#else
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);
#endif


ArduinoTapTempo tapTempo;
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
Scheduler scheduler; // to control your personal task
Button startStopButton(BUTTON_STARTSTOP_PIN);
Button tapTempoButton(BUTTON_TAP_TEMPO);

unsigned long microsecondsPerTick = (unsigned long) (1e6 / (tapTempo.getBPM() * 24.0 / 60.0)) ;
boolean running = false;

void sendTick();
void checkButtonPress();
void checkPots();
void setTickInterval();

// Task taskSendTick( microsecondsPerTick, TASK_FOREVER, &sendTick);
Task taskCheckButtonPress( TASK_CHECK_BUTTON_PRESS_INTERVAL, TASK_FOREVER, &checkButtonPress);
Task taskCheckPots( TASK_CHECK_POT_INTERVAL, TASK_FOREVER, &checkPots);
Task taskSetTickInterval( 5000000, TASK_FOREVER, &setTickInterval);


byte smiley[] = {
  B00000,
  B10001,
  B00000,
  B00000,
  B10001,
  B01110,
  B00000,
};


void setup() {
  pinMode(LED_PLAYING_PIN, OUTPUT);
  pinMode(LED_TEMPO_PIN, OUTPUT);
  digitalWrite(LED_PLAYING_PIN, HIGH);
  digitalWrite(LED_TEMPO_PIN, LOW);

  // button inputs
  pinMode(BUTTON_STARTSTOP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_TAP_TEMPO, INPUT_PULLUP);

  tapTempo.setMinBPM(MIN_BPM);
  tapTempo.setMaxBPM(MAX_BPM);

  scheduler.init();

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, smiley);

#ifdef SOFTSERIAL
  Serial.begin(115200);
  SoftSerial.begin(31250);
#endif

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();

  scheduler.addTask(taskCheckButtonPress);
  scheduler.addTask(taskCheckPots);
  // scheduler.addTask(taskSendTick);
  scheduler.addTask(taskSetTickInterval);
  taskCheckButtonPress.enable() ;
  taskCheckPots.enable();
  // taskSendTick.enable();

  FlexiTimer2::set((long)microsecondsPerTick/10,1.0/100000, sendTick);
  FlexiTimer2::start(); // enable timer interrupt

  newBpmSet();

#ifdef SOFTSERIAL
  Serial.println("sendTick: " + String(taskSendTick.isEnabled()) + " " + String(taskSendTick.getInterval()));
  Serial.println("taskCheckButtonPress: " + String(taskCheckButtonPress.isEnabled()) + " " + String(taskCheckButtonPress.getInterval()));
  Serial.println("taskCheckPots: " + String(taskCheckPots.isEnabled()) + " " + String(taskCheckPots.getInterval()));
#endif
}

void loop() {
  scheduler.execute();
}


// TASK FUNCTIONS
void sendTick() {
  static uint8_t  ticks = 0;

  MIDI.sendRealTime(MIDI_NAMESPACE::Clock);

  ticks++;
  if(ticks < 6)
  {
    digitalWrite(LED_TEMPO_PIN, LOW);
  }
  else if(ticks == 6)
  {
    digitalWrite(LED_TEMPO_PIN, HIGH);
  }
  else if(ticks >= 24)
  {
    ticks = 0;
  }

}

// void tempoLed() {
//   static boolean lastTapTempoLed = false;
//
//   // Fucking state machine
//   if( 0.95 < tapTempo.beatProgress() < 1.0 && ! lastTapTempoLed ) {
//     lastTapTempoLed = true ;
//     digitalWrite(LED_TEMPO_PIN, LOW);
//   } else {
//     lastTapTempoLed = false ;
//   }
//
//   digitalWrite(LED_TEMPO_PIN, HIGH);
// }

void checkButtonPress()
{
  startStopButton.read();
  tapTempoButton.read();

  if( tapTempoButton.wasPressed() ) {
    tapTempo.update(true); // update ArduinoTapTempo
#ifdef SOFTSERIAL
    Serial.println("pressed");
#endif

    newBpmSet();
  } else {
    tapTempo.update(false);
  }

  if( startStopButton.wasPressed() ) {
    sendStartStop();
  }
}

// https://www.norwegiancreations.com/2015/10/tutorial-potentiometers-with-arduino-and-filtering/
void checkPots()
{
  uint32_t pot_val;
  static uint32_t old_pot_val;
  float newBpm ;
  float EMA_a = 0.5;      //initialization of EMA alpha
  static uint32_t EMA_S = 0;          //initialization of EMA S

  pot_val = analogRead(PIN_TEMPO_POT);
  EMA_S = (EMA_a*pot_val) + ((1-EMA_a)*EMA_S);

  if( EMA_S != old_pot_val ) {
    old_pot_val = EMA_S ;
    newBpm = mapFloat((float)EMA_S, (float)0, (float)1022, (float)MIN_BPM, (float)MAX_BPM);
    tapTempo.setBPM(newBpm);
#ifdef SOFTSERIAL
    Serial.println("Potval: " + String(EMA_S) + " BPM: " + String(newBpm) + " tapTempo BPM: " + String(tapTempo.getBPM()));
#endif
    newBpmSet();
  }
}

// OCCASIONALLY CALLED FUNCTIONS
void sendStartStop(){
  if( running ) {
#ifdef SOFTSERIAL
    Serial.println("Stopping");
#endif
    running = false;
    MIDI.sendRealTime(MIDI_NAMESPACE::Stop);
    digitalWrite(LED_PLAYING_PIN, HIGH);
    writeToLcd();

  } else {
#ifdef SOFTSERIAL
    Serial.println("Starting");
#endif
    running = true;
    MIDI.sendRealTime(MIDI_NAMESPACE::Start);
    digitalWrite(LED_PLAYING_PIN, LOW);
    writeToLcd();
  }
}

void newBpmSet() {
    writeToLcd();
    microsecondsPerTick = (unsigned long) (1e6 / (tapTempo.getBPM() * 24.0 / 60.0)) ;
    taskSetTickInterval.enableDelayed(100000);
}

void setTickInterval() {
  FlexiTimer2::stop();
  FlexiTimer2::set((long)microsecondsPerTick/10,1.0/100000, sendTick);
  FlexiTimer2::start();
  taskSetTickInterval.disable();
  lcd.setCursor(15,1);
  lcd.print("s");
}

void writeToLcd() {
  lcd.clear();
  lcd.setCursor(1,0);
  lcd.print(String(microsecondsPerTick) + " us");
  lcd.setCursor(1,1);
  lcd.print(String(tapTempo.getBPM()) + " BPM");
  if( running ) {
    lcd.setCursor(15,0);
    lcd.write(0);

  }
}

// HELPER FUNCTIONS
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
