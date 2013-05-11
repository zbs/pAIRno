/*
2-12-2011
Spark Fun Electronics 2011
Nathan Seidle
Updated to Arduino 1.01 by Marc "Trench" Tschudin

This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

This code works with the VS1053 Breakout Board and controls the VS1053 in what is called Real Time MIDI mode. 
To get the VS1053 into RT MIDI mode, power up the VS1053 breakout board with GPIO0 tied low, GPIO1 tied high.

I use the NewSoftSerial library to send out the MIDI serial at 31250bps. This allows me to print regular messages
for debugging to the terminal window. This helped me out a ton.

5V : VS1053 VCC
GND : VS1053 GND
D3 (SoftSerial TX) : VS1053 RX
D4 : VS1053 RESET

Attach a headphone breakout board to the VS1053:
VS1053 LEFT : TSH
VS1053 RIGHT : RSH
VS1053 GBUF : GND

When in the drum bank (0x78), there are not different instruments, only different notes.
To play the different sounds, select an instrument # like 5, then play notes 27 to 87.

To play "Sticks" (31):
talkMIDI(0xB0, 0, 0x78); //Bank select: drums
talkMIDI(0xC0, 5, 0); //Set instrument number
//Play note on channel 1 (0x90), some note value (note), middle velocity (60):
noteOn(0, 31, 60);

*/

#include <SoftwareSerial.h>
#include <DistanceGP2Y0A21YK.h>
#include "SPI.h"
#include "WS2801.h"

SoftwareSerial mySerial(2, 3); // RX, TX

byte note = 0; //The MIDI note value to be played
byte resetMIDI = 4; //Tied to VS1053 Reset line
byte ledPin = 13; //MIDI traffic inidicator
int  instrument = 0;

const int SENSOR_PINS[] = {A0, A1, A2};
DistanceGP2Y0A21YK sensors[sizeof(SENSOR_PINS)/sizeof(int)];

boolean isSensorOn[] = {false, false, false};
//one octave down
const int SENSOR_NOTES[] = {48, 50, 52};
// Middle C, D, and E
const int SENSOR_NOTES_MED_PITCH[]= {60, 62, 64};
//One Octaves up
const int SENSOR_NOTES_HIGH_PITCH[]= {72, 74, 76};
const int PIANO_NUMBER = 0;
//for back notes
const int MAX_READ_DISTANCE[] = {10, 15, 20};

//which pitch to read
int OLD_PITCH=-1;
int NEW_PITCH=-1;

//for lights
//green is data blue is clock
int dataPin = 9;
int clockPin = 10;

WS2801 strip = WS2801(8, dataPin, clockPin);

const int LED_LOCATION[]={3, 4, 5};
//long is 32 bit variablie 
//color equation= 65536 * r + 256 * g+ b
const long LED_COLOR_LOW_PITCH[]={16711680, 65280, 255};
const long LED_COLOR_MED_PITCH[]={15143680, 6332160, 4194527};
const long LED_COLOR_HIGH_PITCH[]={14491904, 9269760 , 6750320};


void setup() {
	Serial.begin(57600);

	for (int i = 0; i < sizeof(SENSOR_NOTES) / sizeof(int); i++) {
		sensors[i].begin(SENSOR_PINS[i]);
	}

	//Setup soft serial for MIDI control
	mySerial.begin(31250);

	//Reset the VS1053
	pinMode(resetMIDI, OUTPUT);
	digitalWrite(resetMIDI, LOW);
	delay(100);
	digitalWrite(resetMIDI, HIGH);
	delay(100);
	talkMIDI(0xB0, 0x07, 120); //0xB0 is channel message, set channel volume to near max (127)

        //make sure all lights are off
         strip.begin();
         strip.show();l
}


int getPitch(DistanceGP2Y0A21YK sensor){
	for(int i=0; i <sizeof(MAX_READ_DISTANCE); i++){
           if( sensor.getDistanceCentimeter()<MAX_READ_DISTANCE[i]){
             return i;
           } 
        }
    return -1;
}


void loop() {
	talkMIDI(0xB0, 0, 0x00);
	talkMIDI(0xC0, PIANO_NUMBER, 0); //Set instrument number. 0xC0 is a 1 data byte command

	for (int i = 0; i < sizeof(SENSOR_PINS) / sizeof(int); i++) {
                NEW_PITCH=getPitch(sensors[i]);
                if (OLD_PITCH!=NEW_PITCH){
                  oldOff(i);
                  newOn(i);                
                  OLD_PITCH= NEW_PITCH;
                }                 
	}
}


//turn old notes off
void oldOff(int i){
              switch(OLD_PITCH){
                   case -1:          
                      break;
                    case 0:
                      noteOff(0, SENSOR_NOTES[i], 60);
                      break;
                    case 1:
                      noteOff(0, SENSOR_NOTES_MED_PITCH[i], 60);
                      break;
                    case 2:
                      noteOff(0, SENSOR_NOTES_HIGH_PITCH[i], 60);
                      break;
                  }
}

//turn new note on
void newOn(int i){
         switch(NEW_PITCH){
                    case -1:
                      strip.setPixelColor(LED_LOCATION[i], 0, 0, 0);
                      break;
                    case 0:
                      playLowKey(i);
                      break;
                    case 1:
                      playMiddleKey(i);
                      break;
                    case 2:
                      playHighKey(i);
                      break;
                  }
                  
                  strip.show();

}

//play highest pitch
void playLowKey(int i){
   noteOn(0, SENSOR_NOTES[i], 60);
   strip.setPixelColor(LED_LOCATION[i], LED_COLOR_LOW_PITCH[i]);	
}

//play middle pitch
void playMiddleKey(int i){
    noteOn(0, SENSOR_NOTES_MED_PITCH[i], 60);
    strip.setPixelColor(LED_LOCATION[i], LED_COLOR_MED_PITCH[i]);				
}

//play lowest pitch
void playHighKey(int i){
  noteOn(0, SENSOR_NOTES_HIGH_PITCH[i], 60);
  strip.setPixelColor(LED_LOCATION[i], LED_COLOR_HIGH_PITCH[i]);
}

//Send a MIDI note-on message.  Like pressing a piano key
//channel ranges from 0-15
void noteOn(byte channel, byte note, byte attack_velocity) {
	talkMIDI( (0x90 | channel), note, attack_velocity);
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(byte channel, byte note, byte release_velocity) {
	talkMIDI( (0x80 | channel), note, release_velocity);
}

//Plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that data values are less than 127
void talkMIDI(byte cmd, byte data1, byte data2) {
	digitalWrite(ledPin, HIGH);
	mySerial.write(cmd);
	mySerial.write(data1);

	//Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
	//(sort of: http://253.ccarh.org/handout/midiprotocol/)
	if( (cmd & 0xF0) <= 0xB0)
		mySerial.write(data2);

	digitalWrite(ledPin, LOW);
}
