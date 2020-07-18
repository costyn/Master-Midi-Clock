#define _TASK_MICRO_RES
// #define _TASK_TIMECRITICAL
// #define SOFTSERIAL

#include <masterclock.h>


#ifdef SOFTSERIAL
#include <SoftwareSerial.h>
#endif

#define MIN_BPM 30
#define MAX_BPM 180

#define LED_PLAYING_PIN 6
#define LED_TEMPO_PIN 7
#define BUTTON_STARTSTOP_PIN 2
#define BUTTON_CHANGE_POT_PIN 3
#define BUTTON_TAP_TEMPO_PIN 4

#define PIN_TEMPO_POT1 1
#define PIN_TEMPO_POT2 0

#define TASK_CHECK_BUTTON_PRESS_INTERVAL    50000L // 50 milliseconds
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
Button startStopButton(BUTTON_STARTSTOP_PIN, 25, true, false);
Button tapTempoButton(BUTTON_TAP_TEMPO_PIN, 10, true, false);
Button changePotButton(BUTTON_CHANGE_POT_PIN, 25, true, false);

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
  pinMode(BUTTON_TAP_TEMPO_PIN, INPUT_PULLUP);
  pinMode(BUTTON_CHANGE_POT_PIN, INPUT_PULLUP);

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
  // scheduler.addTask(taskSetTickInterval);
  taskCheckButtonPress.enable() ;
  taskCheckPots.enable();

  FlexiTimer2::set((long)microsecondsPerTick/100,1.0/10000, sendTick);
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

  // if( changePotButton.isPressed() ) {
  //   checkPots();
  // }
}

// https://www.norwegiancreations.com/2015/10/tutorial-potentiometers-with-arduino-and-filtering/
void checkPots(){

  static uint32_t Pot1Old;
  static uint32_t Pot1EMA_S = 0;          //initialization of EMA S

  static uint32_t Pot2Old;
  static uint32_t Pot2EMA_S = 0;          //initialization of EMA S

  float newBpm ;
  float Pot1EMA_A = 0.5;      //initialization of EMA alpha
  float Pot2EMA_A = 0.5;      //initialization of EMA alpha
  
  uint32_t Pot1Current = analogRead(PIN_TEMPO_POT1);
  Pot1EMA_S = (Pot1EMA_A*Pot1Current) + ((1-Pot1EMA_A)*Pot1EMA_S);

  uint32_t Pot2Current = analogRead(PIN_TEMPO_POT2);
  Pot2EMA_S = (Pot2EMA_A*Pot2Current) + ((1-Pot2EMA_A)*Pot2EMA_S);

  if( Pot1EMA_S != Pot1Old || Pot2EMA_S != Pot2Old ) {
    Pot1Old = Pot1EMA_S ;
    Pot2Old = Pot2EMA_S ;

    uint16_t wholeNumber = map(Pot1EMA_S, 0, 1022, MIN_BPM, MAX_BPM);
    uint16_t decimal = mapFloat((float)Pot2EMA_S, (float)0, (float)1022, 0, 99);

    // lcd.clear();
    // lcd.setCursor(0,0);
    // lcd.print(wholeNumber);
    // lcd.setCursor(0,1);
    // lcd.print(decimal);

    newBpm = (float)wholeNumber+((float)decimal/100);
    tapTempo.setBPM(newBpm);
#ifdef SOFTSERIAL
    Serial.println("Potval: " + String(Pot1EMA_S) + " BPM: " + String(newBpm) + " tapTempo BPM: " + String(tapTempo.getBPM()));
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
    microsecondsPerTick = (unsigned long) (1e6 / (tapTempo.getBPM() * 24.0 / 60.0)) ;
    writeToLcd();
    // taskSetTickInterval.enableDelayed(100000);
    setTickInterval();
}

void setTickInterval() {
  
  FlexiTimer2::stop();
  FlexiTimer2::set((long)microsecondsPerTick/100,1.0/10000, sendTick);
  FlexiTimer2::start();
  // taskSetTickInterval.disable();
  lcd.setCursor(15,1);
  lcd.print("s");
}

void writeToLcd() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(String((float)microsecondsPerTick/1000) + " " + String(tapTempo.getBeatLength()));
  lcd.setCursor(0,1);
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
