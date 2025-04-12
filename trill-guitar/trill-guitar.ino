#include <Arduino.h>
#include <BLEMidi.h>
#include <Trill.h>
#include <HootBeat.h>

#define PINL 32
#define NUMLEDS 13

// Configurationflags:
const bool debug = false;
const bool debugSensors = false;
const bool mariposa = false;

bool BTconnected = false;

Trill trillSensor;
bool touchActive = false;
bool threeTouchSw = false;

int count = 0;
int prevOutVal = -1;
int prevPushVal = 1;
int pushPin = 5;
const int CCchannel = 1;
bool invert = false;

int someOn = 0;
uint8_t bounceAnim = 10; // "Bounce"
uint8_t someOnAnim = 9; // "Some on"
uint8_t strobeAnim = 7; // "Some on"
uint8_t anim = bounceAnim;
uint8_t currentAnim = bounceAnim;
uint32_t defaultColor      = 0x9060FF,
         disconnColor   = 0x0000F0,
         pushColor      = 0xF0F0F0;

HootBeat hb = HootBeat(NUMLEDS, PINL);

void setup() {
  hb.setDelay(0);
  hb.setColor(disconnColor);
  Serial.begin(115200);
  pinMode(pushPin, INPUT_PULLUP);

  if(mariposa) invert = false;
  else invert = true;
  
  // Trill sensor:
  int ret = Wire.setPins(23, 19);  // Set SDA and SCL for LOLIN32
  // Trill Sensor cable yellow = SCL
  // Trill Sensor cable blue/white = SDA
  ret = trillSensor.setup(Trill::TRILL_BAR);

  if (ret != 0) {
    Serial.println("failed to initialise trillSensor");
    Serial.print("Error code: ");
    Serial.println(ret);
  }

  // BLE MIDI:
  if(mariposa) BLEMidiServer.begin("TrillGuitar");
  else BLEMidiServer.begin("TrillGuitar2");

  BLEMidiServer.setOnConnectCallback([](){
    Serial.println("Connected");
    BTconnected = true;
    hb.setColor(defaultColor);
    anim = someOnAnim;
    currentAnim = someOnAnim;
  });

  BLEMidiServer.setOnDisconnectCallback([]() {  // To show how to make a callback with a lambda function
    Serial.println("Disconnected");
    BTconnected = false;
    hb.setColor(disconnColor);
    hb.isRunning = true;
    anim = bounceAnim;
    currentAnim = bounceAnim;
  });
  //BLEMidiServer.enableDebugging();

}

void loop() {
  delay(50);
  trillSensor.read();
  int numTouches = trillSensor.getNumTouches();
  if (numTouches > 0) {
    if (numTouches == 3 && !threeTouchSw) {
      ccSend(2, 127, CCchannel);
      threeTouchSw = true;
    }

    if (!touchActive) {
      ccSend(0, 127, CCchannel);
    }
    int inVal = trillSensor.touchLocation(0);
    int outVal = midiMapAndClamp(inVal, 0, 3200, 170, 3030, invert);
    someOn = map(inVal, 0, 3030, NUMLEDS, 0);
    if (prevOutVal != outVal) {
      ccSend(1, outVal, CCchannel);
      hb.setSomeOn(someOn);
      prevOutVal = outVal;
    }
    if (debugSensors) {
      for (int i = 0; i < numTouches; i++) {
        Serial.print(trillSensor.touchLocation(i));
        Serial.print(" ");
        Serial.print(trillSensor.touchSize(i));
        Serial.print(" ");
      }
      Serial.println("");
    }
    touchActive = true;
  } else if (touchActive) {
    // Print a single line when touch goes off
    if (debug) Serial.println("0 0");
    ccSend(0, 0, CCchannel);
    ccSend(1, 0, CCchannel); // return to zero?
    prevOutVal = -1; // Reset previous value sent
    touchActive = false;
    if (threeTouchSw) {
      ccSend(2, 0, CCchannel);
      threeTouchSw = false;
    }
    hb.setSomeOn(0);
    hb.triggerFlash(5);
  }

  int pushVal = digitalRead(pushPin);
  if(prevPushVal != pushVal) {
    ccSend(3, (1-pushVal)*127, CCchannel);
    prevPushVal = pushVal;
    if(pushVal == 0) {
      hb.setColor(pushColor);
      anim = strobeAnim;
    } else {
      hb.setColor(defaultColor);
      anim = currentAnim;
    }
  }

  hb.step(anim);
}

void ccSend(int cc, int value, int channel) {
  if (debug) {
    debugThis("cc", cc, value);
  } else {
    BLEMidiServer.controlChange(channel, cc, value);
  }
}

void noteOnSend(int note, int vel, int channel) {
  if (debug) {
    debugThis("noteOn", note, vel);
  } else {
    BLEMidiServer.noteOn(channel, note, vel);
  }
}

void noteOffSend(int note, int vel, int channel) {
  if (debug) {
    debugThis("noteOff", note, vel);
  } else {
    BLEMidiServer.noteOff(channel, note, vel);
  }
}

int midiMapAndClamp(int input, int min, int max, int bottom, int top, bool inv) {
  if (input < bottom) input = bottom;
  if (input > top) input = top;
  int outval = 0;
  if (inv) outval = map(input, bottom, top, 127, 0);
  else outval = map(input, bottom, top, 0, 127);
  return (outval);
}

void debugThis(String name, int i, int value) {
  if (debug) {
    Serial.print(name);
    Serial.print("[");
    Serial.print(i);
    Serial.print("]");
    Serial.print(": ");
    Serial.println(value);
  }
}