/*TODO:
- Make rocker switch work -- i.e. reset circuit when user turns off and on
- Incorporate instrument into noteinstance so that you can play different instruments across a recording

*/
#include "types.h"
#include <SoftwareSerial.h>
#include <DistanceGP2Y0A21YK.h>
#include <QueueList.h>

enum NoteMode {
	TURN_ON,
	TURN_OFF
};
struct NoteInstance {
	int note;
	int time;
	NoteMode mode;
};



SoftwareSerial mySerial(2, 3); // RX, TX

byte note = 0; //The MIDI note value to be played
byte resetMIDI = 4; //Tied to VS1053 Reset line
byte ledPin = 13; //MIDI traffic inidicator

const int SENSOR_PINS[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11};
const int NUM_SENSORS = sizeof(SENSOR_PINS)/sizeof(int);
DistanceGP2Y0A21YK sensors[sizeof(SENSOR_PINS)/sizeof(int)];

boolean isSensorOn[] = {false, false, false};

const int MAX_READ_DISTANCE[] = {10, 15, 20};

// Middle C, D, and E

struct PitchTrio {
	int lowPitch;
	int mediumPitch;
	int highPitch;
};

struct LightTrio {
	int lowLight;
	int mediumLight;
	int highLight;
};

const long LED_COLOR_LOW_PITCH[]={16711680, 65280, 255};
const long LED_COLOR_MED_PITCH[]={15143680, 6332160, 4194527};
const long LED_COLOR_HIGH_PITCH[]={14491904, 9269760 , 6750320};

const int SENSOR_NOTES_LOW_PITCH[] = {48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70};
const int SENSOR_NOTES_MED_PITCH[]= {60, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80, 82};
const int SENSOR_NOTES_HIGH_PITCH[]= {72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94};

int currentNotes[NUM_SENSORS];

QueueList<NoteInstance> noteLoop;
QueueList<NoteInstance> bufferLoop;
unsigned long recordTimeCounter;

int noteIndex = 0;

enum ProcessMode {
	ACTIVE,
	INACTIVE
};

enum Instrument {
	GRAND_PIANO = 0,
	TENOR_SAX = 66,
	HARPSICHORD = 6,
	BANJO = 105
};



const int FOURWAY_SWITCH_PINS[] = {42, 43, 44, 45};
const Instrument FOURWAY_SWITCH_INTSTRUMENTS[] = {GRAND_PIANO, TENOR_SAX, HARPSICHORD, BANJO};

Instrument currentInstrument = GRAND_PIANO;
ProcessMode recordPedalMode = INACTIVE;
ProcessMode playButtonMode = INACTIVE;
ProcessMode loopPedalMode = INACTIVE;

unsigned long currentTimeCounter = 0;

const int NUM_PEDALS = 1;
// the current reading from the input pin

long DEBOUNCE_DELAY = 50;

struct Button {
	const int pin;
	int state;
	int lastState;
	long lastDebounceTime;
};

Button recordPedal = {53, LOW, LOW, 0};
Button playButton = {52, LOW, LOW, 0};
bool playToggle = false;

void setup() {
	Serial.begin(9600);
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
	//Set everything up for channel 1
	talkMIDI(0xB1, 0x07, 120); //0xB0 is channel message, set channel volume to near max (127)

	//Set up pedals
	pinMode(recordPedal.pin, INPUT);
}

RangeHeight range(DistanceGP2Y0A21YK sensor){
	int distance = sensor.getDistanceCentimeter();

	for (int i = 0; i < sizeof(MAX_READ_DISTANCE)/sizeof(int); i++) {
		if (distance < MAX_READ_DISTANCE[i]) {
			return (RangeHeight)i;
		}
	}
	
	return INVALID;
}

void clearNoteLoop(){
	while (!noteLoop.isEmpty()) {
		noteLoop.pop();
	}
}

void resetNoteLoop(){
	while (noteIndex % noteLoop.count() != 0){
		noteLoop.push(noteLoop.pop());
		noteIndex++;
	}
}

void checkRecordingPedal() {
	int reading = digitalRead(recordPedal.pin);

	if (reading != recordPedal.lastState) {
		recordPedal.lastDebounceTime = millis();
	} 

	if ((millis() - recordPedal.lastDebounceTime) > DEBOUNCE_DELAY) {
		if (recordPedal.state != reading && recordPedalMode == INACTIVE) {
			Serial.println("Start recording");
			recordTimeCounter = millis();
			loopPedalMode = INACTIVE;
			recordPedalMode = ACTIVE;
			clearNoteLoop();
		}
		else if (recordPedal.state != reading && recordPedalMode == ACTIVE){
			Serial.println("Stop recording");
			recordPedalMode = INACTIVE;

			// Playback code

			//printQueue();
		}
		recordPedal.state = reading;
	}
	recordPedal.lastState = reading;
}

void checkPlayButton() {
	int reading = digitalRead(playButton.pin);

	if (reading != playButton.lastState) {
		playButton.lastDebounceTime = millis();
	} 

	if ((millis() - playButton.lastDebounceTime) > DEBOUNCE_DELAY) {
		if (playButton.state != reading && playButtonMode == INACTIVE) {
			playButtonMode = ACTIVE;
		}
		else if (playButton.state != reading && playButtonMode == ACTIVE){
			playButtonMode = INACTIVE;
			playToggle = !playToggle;

			if (playToggle) {
				Serial.println("Playback started");
				loopPedalMode = ACTIVE;
				currentTimeCounter = 0;
				resetNoteLoop();
				noteIndex = 0;
			}
			else {
				loopPedalMode = INACTIVE;
				Serial.println("Playback stopped");
			}
		}
		playButton.state = reading;
	}
	playButton.lastState = reading;
}


void printQueue(){
	while (!noteLoop.isEmpty()) {
		Serial.println(noteLoop.pop().time);
	}
}

void checkFourWaySwitch(){
	for (int i = 0; i < sizeof(FOURWAY_SWITCH_PINS)/sizeof(int); i++) {
		if (digitalRead(FOURWAY_SWITCH_PINS[i]) == HIGH) {
			currentInstrument = FOURWAY_SWITCH_INTSTRUMENTS[i];
			return;
		}
	}
}

int getNoteBySensorAndRange(int sensor, RangeHeight height){
	switch (height) {
	case LOW_HEIGHT:
		return SENSOR_NOTES_LOW_PITCH[sensor];
	case MEDIUM_HEIGHT:
		return SENSOR_NOTES_MED_PITCH[sensor];
	case TALL_HEIGHT:
		return SENSOR_NOTES_HIGH_PITCH[sensor];
	case INVALID:
		return -1;
	}
}

void loop() {
	checkFourWaySwitch();

	talkMIDI(0xB0, 0, 0x00);
	talkMIDI(0xC0, currentInstrument, 0); //Set instrument number. 0xC0 is a 1 data byte command

	// Do the same for channel 1
	talkMIDI(0xB1, 0, 0x00);
	talkMIDI(0xC1, currentInstrument, 0); //Set instrument number. 0xC0 is a 1 data byte command

	checkRecordingPedal();
	checkPlayButton();
	//checkLoopingPedal();

	if (loopPedalMode == ACTIVE) {
		if (!noteLoop.isEmpty() && 100* (noteLoop.peek().time / 100) == currentTimeCounter) {
			noteIndex++;
			Serial.print("NOTE HIT:");
			if (noteIndex % noteLoop.count() == 0) {
				Serial.println("RESTART POINT");
				currentTimeCounter = 0;
			}

			NoteInstance note = noteLoop.pop();
			noteLoop.push(note);

			Serial.print(note.note);
			Serial.print(" : ");
			Serial.println(note.mode);
			if (note.mode == TURN_ON) {
				Serial.println("TURN NOTE ON");
				noteOn(1, note.note, 60);
				delay(50);
			}
			else {
				Serial.println("TURN NOTE OFF");
				noteOff(1, note.note, 60);
				delay(50);
			}
		}
		currentTimeCounter+= 50;
	}

	for (int i = 0; i < sizeof(SENSOR_PINS) / sizeof(int); i++) {
		RangeHeight height = range(sensors[i]);
		if ( height != INVALID){
			if (!isSensorOn[i]) {
				Serial.println("Sensor on");
				currentNotes[i] = getNoteBySensorAndRange(i, height);
				noteOn(0, currentNotes[i], 60);
				delay(50);
				isSensorOn[i] = true;

				if (recordPedalMode == ACTIVE) {
					NoteInstance noteInstance = {currentNotes[i], millis() - recordTimeCounter, TURN_ON};
					noteLoop.push(noteInstance);
				}
				Serial.println(noteLoop.count());
			}
		}
		else {
			if (isSensorOn[i]) {
				Serial.println("Sensor off");
				noteOff(0, currentNotes[i], 60);
				isSensorOn[i] = false;
				delay(50);
				if (recordPedalMode == ACTIVE) {
					NoteInstance noteInstance = {currentNotes[i], millis() - recordTimeCounter, TURN_OFF};
					noteLoop.push(noteInstance);
				}
			}
		}
	}
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