#include <Arduino.h>
#include <BLEMidi.h>
#include <Button2.h>
#include <Adafruit_NeoPixel.h>

#define POT_COUNT 4 // We have 4 potentiometers/knobs
#define BUT_COUNT 4
#define INTLED 22
#define LONGCLICK_MS 5000

#define LED_PIN 17
#define NUMLEDS 2

// Configurationflags:
const bool debug = false;
const bool usbMIDI = true; // Send MIDI via USB?
const bool sendRelease = true; // Send on release? 
bool pickUpMode = true;
bool BTconnected = false;
static unsigned long last = 0;

Button2 sw_1 = Button2(27, INPUT_PULLUP);
Button2 sw_2 = Button2(18, INPUT_PULLUP);
Button2 sw_3 = Button2(23, INPUT_PULLUP);
Button2 sw_4 = Button2(19, INPUT_PULLUP);

Adafruit_NeoPixel strip  = Adafruit_NeoPixel(NUMLEDS, LED_PIN);

int count = 0;
bool ledStatus = false, noteOnSent = false;
const int CCchannel = 1;
const int INchannel = 16;
const int PCchannel = 13;
const int note[BUT_COUNT] = {31,30,29,28};

const int cc_pot[POT_COUNT] = {3, 4, 5, 6};
const int pot[POT_COUNT] = {35,34,33,32};
int potval[POT_COUNT];
int potvalIN[POT_COUNT];
bool potPosCorrect[POT_COUNT] = {true, true, true, true};
uint32_t color = 0xFF00FF;
int colorCount = 0;
int maxCount = 5;
float inVel;

void connected()
{
  Serial.println("Connected");
  BTconnected = true;
}

void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{
  //Serial.printf("Received note on : channel %d, note %d, velocity %d (timestamp %dms)\n", channel, note, velocity, timestamp);
  inVel = (float) velocity / 127;
  colorCount = maxCount;
}

void setup() {
  pinMode(INTLED, OUTPUT);
  Serial.begin(115200);
  BLEMidiServer.begin("Transparent");
  BLEMidiServer.setOnConnectCallback(connected);
  BLEMidiServer.setOnDisconnectCallback([](){     // To show how to make a callback with a lambda function
    Serial.println("Disconnected");
    BTconnected = false;
  });
  BLEMidiServer.setNoteOnCallback(onNoteOn);
  //BLEMidiServer.enableDebugging();

  for(int i=0; i<4; i++) {
    adcAttachPin(pot[i]);
    potval[i] = mapAndClamp(analogRead(pot[i]));
  }

  // Switch press and release callbacks
  sw_1.setPressedHandler(onButtonPressed);
  //sw_1.setLongClickDetectedHandler(longClick);
  sw_2.setPressedHandler(onButtonPressed);
  sw_3.setPressedHandler(onButtonPressed);
  sw_4.setPressedHandler(onButtonPressed);

  if(sendRelease) {
    sw_1.setReleasedHandler(onButtonReleased);
    sw_2.setReleasedHandler(onButtonReleased);
    sw_3.setReleasedHandler(onButtonReleased);
    sw_4.setReleasedHandler(onButtonReleased);  
  }
  analogReadResolution(10);

  strip.begin();
  strip.setBrightness(100);

  for(uint8_t i=0; i<NUMLEDS; i++) {
     strip.setPixelColor (i, 0x000000);
  }
  strip.show();
}

void loop() {

  if(BTconnected) digitalWrite(INTLED, LOW);
  else {
    if(count >= 1000) {
      ledStatus = !ledStatus;
      digitalWrite(INTLED, ledStatus);
      count = 0;
    }
    count++;
  }
  sw_1.loop();
  sw_2.loop();
  sw_3.loop();
  sw_4.loop();

  int potvalNew[4];
  for(int i=0; i<4; i++) {
    potvalNew[i] = mapAndClamp(analogRead(pot[i]));
  }

  for(int i=0; i<4; i++) {
    int diff = abs(potval[i] - potvalNew[i]);
    if( diff > 1) {
      potval[i] = potvalNew[i];
      ccSend(cc_pot[i], potvalNew[i], CCchannel);
    }
  }
  /*--------- LEDs BEGIN ---------*/
  if ( BLEMidiServer.isConnected() && (millis() - last >= 24) ) {
    last += 24;
    uint8_t  i;
    for(i=0; i<NUMLEDS; i++) {
      uint32_t c = 0;
      float fade = ((float) colorCount / maxCount) * inVel;
      fade *= fade;
      if(colorCount > 0) {
        c = dimColor(color, fade);
        //Serial.println(fade);
      }
      strip.setPixelColor (i, c);
    }
    colorCount--;
    strip.show();
  }
  /*--------- LEDs END ---------*/
  
}

void onButtonPressed(Button2& btn){
  int id = btn.getID();
  noteOnSend(note[id], 127, CCchannel);
}

void onButtonReleased(Button2& btn){
  int id = btn.getID();
  noteOffSend(note[id], 0, CCchannel);
}

void ccSend(int cc, int value, int channel) {
  if(debug) {
    debugThis("cc", cc, value);
  } else {
    BLEMidiServer.controlChange(channel, cc, value);
  }
}

void noteOnSend(int note, int vel, int channel) {
  if(debug) {
    debugThis("noteOn", note, vel);
  } else {
    BLEMidiServer.noteOn(channel, note, vel);
  }
}

void noteOffSend(int note, int vel, int channel) {
  if(debug) {
    debugThis("noteOff", note, vel);
  } else {
    BLEMidiServer.noteOff(channel, note, vel);
  }
}

int mapAndClamp(int input) {
  int inMin = 1023;
  int inMax = 0;
  // Invert (0-127) range if needed
  int outval = map(input, inMin, inMax, 127, 0);
  if(outval < 0) outval = 0;
  if(outval>127) outval = 127;
  return (outval);
}

int isPotCC(int val) {
  for(int i=0; i<POT_COUNT; i++) {
    if(cc_pot[i] == val)
      return (i);
  }
  return (-1);
}

void debugThis(String name, int i, int value) {
  if(debug) {
    Serial.print(name);
    Serial.print("[");
    Serial.print(i);
    Serial.print("]");
    Serial.print(": ");
    Serial.println(value);
  }
}

uint32_t dimColor(uint32_t color, float fade) {
    uint8_t r = fade * (float) ((color >> 16) & 0x0000FF);
    uint8_t g = fade * (float) ((color >> 8) & 0x0000FF);
    uint8_t b = fade * (float) (color & 0x0000FF);
    uint32_t dimmedColor = (r<<16) + (g<<8) + (b);
    return (dimmedColor);
}
