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
const int  instrument = 0;


const int SENSOR_PINS[] = {A0, A2};
DistanceGP2Y0A21YK sensors[sizeof(SENSOR_PINS)/sizeof(int)];

boolean isSensorOn[] = {false, false, false};

// Middle C, D, and E
const int SENSOR_NOTES[] = {60, 62, 64};
const int PIANO_NUMBER = 0;
const int MAX_READ_DISTANCE = 18;
const int MIN_READ_DISTANCE = 5;

QueueList<NoteInstance> noteLoop;
QueueList<NoteInstance> bufferLoop;
unsigned long recordTimeCounter;

int noteIndex = 0;

enum ProcessMode {
	ACTIVE,
	INACTIVE
};

ProcessMode recordPedalMode = INACTIVE;
ProcessMode loopPedalMode = INACTIVE;

unsigned long currentTimeCounter = 0;

const int NUM_PEDALS = 1;
const int RECORD_PEDAL_PIN = 10;
int recordPedalLastState = LOW;
int recordPedalState = LOW;
  // the current reading from the input pin

long recordLastDebounceTime = 0;  // the last time the output pin was toggled	
long DEBOUNCE_DELAY = 50;

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
	pinMode(RECORD_PEDAL_PIN, INPUT);
}

boolean inRange(DistanceGP2Y0A21YK sensor){
	int distance = sensor.getDistanceCentimeter();
	if ( distance < MAX_READ_DISTANCE && distance > MIN_READ_DISTANCE) {
		return true;
	}
	return false;
}

void clearNoteLoop(){
	while (!noteLoop.isEmpty()) {
		noteLoop.pop();
	}
}

void checkRecordingPedal() {
	int reading = digitalRead(RECORD_PEDAL_PIN);

	if (reading != recordPedalLastState) {
		recordLastDebounceTime = millis();
	} 

	if ((millis() - recordLastDebounceTime) > DEBOUNCE_DELAY) {
		if (recordPedalState != reading && recordPedalMode == INACTIVE) {
			Serial.println("Start recording");
			recordTimeCounter = millis();
			loopPedalMode = INACTIVE;
			recordPedalMode = ACTIVE;
			clearNoteLoop();
		}
		else if (recordPedalState != reading && recordPedalMode == ACTIVE){
			Serial.println("Stop recording");
			recordPedalMode = INACTIVE;
			
			// Playback code
			loopPedalMode = ACTIVE;
			currentTimeCounter = 0;
			noteIndex = 0;
			//printQueue();
		}
		recordPedalState = reading;
	}
	recordPedalLastState = reading;
}

void checkPlaybackPedal() {
	//int reading = digitalRead(playback_PEDAL_PIN);

	//if (reading != playbackPedalLastState) {
	//	playbackLastDebounceTime = millis();
	//} 

	//if ((millis() - playbackLastDebounceTime) > DEBOUNCE_DELAY) {
	//	if (playbackPedalState != reading && playbackPedalMode == INACTIVE) {
	//		currentTimeCounter = 0;
	//		playbackPedalMode = ACTIVE;
			//noteIndex = 0 ;
	//		clearNoteLoop();
	//	}
	//	else if (playbackPedalState != reading && playbackPedalMode == ACTIVE){
	//		playbackPedalMode = INACTIVE;
	//	}
	//	playbackPedalState = reading;
	//}
	//playbackPedalLastState = reading;
}

void printQueue(){
	while (!noteLoop.isEmpty()) {
		Serial.println(noteLoop.pop().time);
	}
}

void loop() {
	talkMIDI(0xB0, 0, 0x00);
	talkMIDI(0xC0, PIANO_NUMBER, 0); //Set instrument number. 0xC0 is a 1 data byte command

	// Do the same for channel 1
	talkMIDI(0xB1, 0, 0x00);
	talkMIDI(0xC1, PIANO_NUMBER, 0); //Set instrument number. 0xC0 is a 1 data byte command

	checkRecordingPedal();
	//checkLoopingPedal();

	/*if (isPlayingActivated()) {
		isPlayingLoop = true;
		currentTimeCounter = 0;
		noteIndex = 0;
	}

	if (isPlayingStopped()) {
		isPlayingLoop = false;
	}*/
	if (loopPedalMode == ACTIVE) {
		//Serial.println("LOOP ACTIVE");
		//Serial.println(currentTimeCounter);
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
		currentTimeCounter+= 100;
	}

	for (int i = 0; i < sizeof(SENSOR_PINS) / sizeof(int); i++) {
		if (inRange(sensors[i])){
			if (!isSensorOn[i]) {
				Serial.println("Sensor on");
				noteOn(0, SENSOR_NOTES[i], 60);
				isSensorOn[i] = true;

				if (recordPedalMode == ACTIVE) {
					NoteInstance noteInstance = {SENSOR_NOTES[i], millis() - recordTimeCounter, TURN_ON};
					noteLoop.push(noteInstance);
				}
				Serial.println(noteLoop.count());
			}
		}
		else {
			if (isSensorOn[i]) {
				Serial.println("Sensor off");
				noteOff(0, SENSOR_NOTES[i], 60);
				isSensorOn[i] = false;

				if (recordPedalMode == ACTIVE) {
					NoteInstance noteInstance = {SENSOR_NOTES[i], millis() - recordTimeCounter, TURN_OFF};
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