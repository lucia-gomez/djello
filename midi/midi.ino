#include <MIDIUSB.h>
#include <movingAvg.h>
#include <Wire.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// touch sensor
Adafruit_MPR121 cap = Adafruit_MPR121();
uint16_t lasttouched = 0;
uint16_t currtouched = 0;
const int JELLO_PIN_START = 0;
const int JELLO_PIN_END = 5;
int jelloHoldTimes[12];

int fsrPin1 = A0;
int fsrPin2 = A1;
int flexPin = A3;
int potPin = A2;

movingAvg avgFsr1(20);
movingAvg avgFsr2(20);
movingAvg avgPot(20);
movingAvg avgFlex(20);

int potPrev = -1;
int songPlaying = 0;

void setup() {
  Serial.begin(9600);
  avgFsr1.begin();
  avgFsr2.begin();
  avgPot.begin();
  avgFlex.begin();

  pinMode(12, OUTPUT);

  while (!Serial) {
    delay(10);
  }
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");

  for(int i = JELLO_PIN_START; i <= JELLO_PIN_END; i++) {
    jelloHoldTimes[i] = 0;
  }
}

void loop() {
  currtouched = cap.touched();

  int fsr1 = analogRead(fsrPin1); 
  int fsr2 = analogRead(fsrPin2); 
  int fsrAvg1 = avgFsr1.reading(fsr1);
  int fsrAvg2 = avgFsr2.reading(fsr2);

  int pot = analogRead(potPin);
  int potAvg = avgPot.reading(pot);
  int flex = analogRead(flexPin);
  int flexAvg = avgFlex.reading(flex);

  int potNote = map(potAvg, 350, 700, 0, 5);

  Serial.print("FSR1: ");
  Serial.print(fsrAvg1);
  Serial.print(", ");
  Serial.print("FSR2: ");
  Serial.print(fsrAvg2);
  Serial.print(", ");
  Serial.print("POT: ");
  Serial.print(potAvg);
  Serial.print(", ");
  Serial.print("POTNOTE: ");
  Serial.print(potNote);
  Serial.print(", ");
  Serial.print("FLEX: ");
  Serial.println(flexAvg);

  int flexThresholdMin = 200;
  int flexThresholdMax = 440;
  if (flexAvg > flexThresholdMax) {
    int flexNote = map(flexAvg, 400, 640, 40, 80);
    Serial.println(flexNote);
    noteOn(9, flexNote, 10);
  }

  if (fsrAvg1 > 70) {
    int fsrMap1 = map(min(80, fsrAvg1), 70, 80, 0, 90);
    sendCCMessage(8, fsrMap1);
  }

  if (fsrAvg2 > 350) {
    int fsrMap2 = map(min(500, fsrAvg2), 350, 500, 0, 127);
    sendCCMessage(11, fsrMap2);
  }
  
  if (potNote != potPrev) {
    noteOn(10, 40 + potNote * 5, 10);
  }
  potPrev = potNote;

  // 0 = big left
  // 1 = 3 right
  // 2 = 3 bottom
  // 3 = big right
  // 4 = by FSRs
  // 5 = 3 left
  for(int i = JELLO_PIN_START; i <= JELLO_PIN_END; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" touched");
      switch(i) {
        case 4:
          if (songPlaying == 0) {
            songPlaying = 1;
            sendMIDIStartPlayback();
          // sendCCMessage(5, 1);
          } else {
            songPlaying = 0;
            sendMIDIStopPlayback();
            // sendCCMessage(6, 0);
          }
          break;
        default:
          jelloHoldTimes[i] = millis();
      }
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" released");
      int note1 = holdTimeToNote(jelloHoldTimes[i], millis());
      switch(i) {
        case 0:
          noteOn(4, 60, 10);
          break;
        case 4:
          break;
        case 3:
          noteOn(3, 60, 10);
          break;
        default:
          noteOn(i, note1, 10);
      }
      jelloHoldTimes[i] = 0;
    }
  }
  
  digitalWrite(12, 1);
  lasttouched = currtouched;
  delay(50);
}

int holdTimeToNote(long start, long end) {
  long diff = end - start;
  int cap = 1500;
  diff = min(diff, cap);
  return map(diff, 0, cap, 70, 30);
}

int holdTimeToPitch(long start, long end) {
  long diff = end - start;
  int cap = 1500;
  diff = min(diff, cap);
  return map(diff, 0, cap, 200, 60);
}

void sendMIDIStopPlayback() {
  byte controlChangeNumber = 0x41;  // Control Change number for start playback
  byte value = 127;  // You can adjust this value as needed

  midiEventPacket_t midiEvent = {0x0B, 0xB0 | 6, controlChangeNumber, value};
  MidiUSB.sendMIDI(midiEvent);
}

void sendMIDIStartPlayback() {
  byte controlChangeNumber = 0x40;  // Control Change number for start playback
  byte value = 127;  // You can adjust this value as needed

  midiEventPacket_t midiEvent = {0x0B, 0xB0 | 6, controlChangeNumber, value};
  MidiUSB.sendMIDI(midiEvent);
}

void sendCCMessage(byte channel, byte value) {
  byte status = 0xB0;
  midiEventPacket_t midiMsg = {status >> 4, status, channel, value};
  MidiUSB.sendMIDI(midiMsg);
}

// channel is not 0 based
void noteOn(byte channel, byte note, byte velocity) {
  byte cmd = 0x90 + (channel - 1);
  midiEventPacket_t midiMsg = {cmd >> 4, cmd, note, velocity};
  MidiUSB.sendMIDI(midiMsg);
}

// channel is not 0 based
void noteOff(byte channel, byte note, byte velocity) {
  byte cmd = 0x80 + (channel - 1);
  midiEventPacket_t midiMsg = {cmd >> 4, cmd, note, velocity};
  MidiUSB.sendMIDI(midiMsg);
}
