#include <Arduino.h>
#include <BLEMidi.h>
#include <Wire.h>
#include <Button2.h>
#include <HootBeat.h>

#define POT_COUNT 4 // We have 4 potentiometers/knobs
#define BUT_COUNT 4

#define I2C_SDA 15
#define I2C_SCL 2

// Configurationflags:
const bool debug = false;
const bool usbMIDI = true; // Send MIDI via USB?
const bool sendRelease = true; // Send on release? 
bool pickUpMode = true;
bool BTconnected = false;

Button2 sw_1 = Button2(27, INPUT_PULLUP);
Button2 sw_2 = Button2(18, INPUT_PULLUP);
Button2 sw_3 = Button2(23, INPUT_PULLUP);
Button2 sw_4 = Button2(19, INPUT_PULLUP);

HootBeat hb = HootBeat(2, 17);

const int intLed = 22;
int count = 0;
bool ledStatus = false;
const int CCchannel = 1;
const int INchannel = 16;
const int PCchannel = 13;
const int note[BUT_COUNT] = {31,30,29,28};

const int cc_pot[POT_COUNT] = {3, 4, 5, 6};
const int pot[POT_COUNT] = {34,35,33,32};
int potval[POT_COUNT];
int potvalIN[POT_COUNT];
bool potPosCorrect[POT_COUNT] = {true, true, true, true};
const bool i2cMIDI = true;
int anim = 5;

void connected()
{
  Serial.println("Connected");
  BTconnected = true;
}

void setup() {
  pinMode(intLed, OUTPUT);
  Serial.begin(115200);
  if(i2cMIDI) Wire.begin(I2C_SDA, I2C_SCL);
  BLEMidiServer.begin("Transparent");
  BLEMidiServer.setOnConnectCallback(connected);
  BLEMidiServer.setOnDisconnectCallback([](){     // To show how to make a callback with a lambda function
    Serial.println("Disconnected");
    BTconnected = false;
  });
  //BLEMidiServer.enableDebugging();

  for(int i=0; i<4; i++) {
    adcAttachPin(pot[i]);
    potval[i] = mapAndClamp(analogRead(pot[i]));
  }

  // Switch press and release callbacks
  sw_1.setPressedHandler(onButtonPressed);
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
  hb.setColor(0xFFFFFF);
  anim = 5;
}

void loop() {
  if(BTconnected) digitalWrite(intLed, LOW);
  else {
    if(count >= 1000) {
      ledStatus = !ledStatus;
      digitalWrite(intLed, ledStatus);
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
  hb.step(anim);
}

void onButtonPressed(Button2& btn){
  int id = btn.getID();
  noteOnSend(note[id], 127, CCchannel);
  if(id==0) hb.setColor(0x9900DD);
  else if(id==1) hb.setColor(0xDDDDDD);
  else if(id==2) hb.setColor(0xDDDD00);
  else if(id==3) hb.setColor(0x0000FF);
  hb.triggerFlash();
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
    i2cSend("cc", cc, value);
  }
}

void noteOnSend(int note, int vel, int channel) {
  if(debug) {
    debugThis("noteOn", note, vel);
  } else {
    BLEMidiServer.noteOn(channel, note, vel);
    i2cSend("noteOn", note, vel);
  }
}

void noteOffSend(int note, int vel, int channel) {
  if(debug) {
    debugThis("noteOff", note, vel);
  } else {
    BLEMidiServer.noteOff(channel, note, vel);
    i2cSend("noteOff", note, vel);
  }
}

int mapAndClamp(int input) {
  int inMin = 1023;
  int inMax = 0;
  int outval = map(input, inMin, inMax, 0, 127);
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

void i2cSend(String type, int val1, int val2) {
  /*
  int eventType = 0x09;
  if(type == "noteOff") eventType = 0x08;
  else if(type == "cc") eventType = 0x0B;
  */
  // Why these numbers???
  int eventType = 117;
  if(type == "noteOff") eventType = 109;
  else if(type == "cc") eventType = 129;

  if(i2cMIDI) {
    Wire.beginTransmission(8);
    Wire.write(eventType);
    Wire.write(val1);
    Wire.write(val2);
    Wire.endTransmission(); 
  }
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
