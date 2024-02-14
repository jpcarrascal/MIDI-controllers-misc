#include <Arduino.h>
#include <BLEMidi.h>
#include <Trill.h>

// Configurationflags:
const bool debug = false;
bool BTconnected = false;

Trill trillSensor;
bool touchActive = false;
bool threeTouchSw = false;

int count = 0;
int prevOutVal = -1;
const int CCchannel = 1;

void connected() {
  Serial.println("Connected");
  BTconnected = true;
}

void setup() {
  Serial.begin(115200);

  // Trill sensor:
  int ret = Wire.setPins(23, 19);  // Set SDL and SDA for LOLIN32
  ret = trillSensor.setup(Trill::TRILL_BAR);

  if (ret != 0) {
    Serial.println("failed to initialise trillSensor");
    Serial.print("Error code: ");
    Serial.println(ret);
  }

  // BLE MIDI:
  BLEMidiServer.begin("TrillGuitar");
  BLEMidiServer.setOnConnectCallback(connected);
  BLEMidiServer.setOnDisconnectCallback([]() {  // To show how to make a callback with a lambda function
    Serial.println("Disconnected");
    BTconnected = false;
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
    int outVal = midiMapAndClamp(inVal, 0, 3200, 170, 3030, false);
    if (prevOutVal != outVal) {
      ccSend(1, outVal, CCchannel);
      prevOutVal = outVal;
    }
    if (debug) {
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
  }
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
  if (min < bottom) min = bottom;
  if (max > top) max = top;
  int outval = 0;
  if (inv) outval = map(input, min, max, 127, 0);
  else outval = map(input, min, max, 0, 127);
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