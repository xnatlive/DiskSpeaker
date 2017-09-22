/*
 Name:		DiskSpeaker.ino
 Created:	8/17/2017 1:37:56 AM
 Author:	Saleh
*/

#include <PT2314.h>
#include <EEPROM.h>

#include <Adafruit_MPR121.h>
#include <IRremote.h>
#include <Wire.h>

// Uncomment this line to enable output to the serial monitor (at 9600 baud).
#define SERIAL_MONITOR

// Sources and Modes
#define SOURCES               3
#define SOURCE_BLUETOOTH      0
#define SOURCE_USB            1
#define SOURCE_LINEIN         2


// EEPROM Addresses
#define SETTINGS_ADDRESS          64
#define SETTINGS_ID_STRING        "AET 1.0"

typedef struct SettingsType {
	char        id[8];
	boolean     standby;
	byte        source;
	//byte        displayMode;
	//byte        graphStyle;
	//signed char currentPreset;
	//int         presetFrequency[PRESETS];
	//int         currentFrequency;
	int         currentVolume;
	int         bass;
	int         treble;
	int         balance;
	//int         brightness;
	//boolean     random;
	//boolean     repeatFolder;
};



//
const int status_LED_pin = 2;
// Pin connected to IR reciver
#define  PIN_IR_RECV            3

//Pin connected to latch pin (ST_CP) of 74HC595
const int latchPin = 5;
//Pin connected to clock pin (SH_CP) of 74HC595
const int clockPin = 7;
////Pin connected to Data in (DS) of 74HC595
const int dataPin = 6;

// 5V Relay for sending a 12V "Remote Power On" signal to am external Amplifier.
#define PIN_REL_POWER            11
#define PIN_REL_SPEAKERS         12


bool IsDeviceON = false;
bool IsLEDsON = false;
bool Mute = false;
int volume = 0;

// For 4 audio inputs, Volume, Bass, Treble, and Balance
#define PT2314_I2C  0x44
PT2314 audio;

IRrecv irrecv(PIN_IR_RECV);
decode_results results;


// the timer object
unsigned long lastTouch;
unsigned long time;

//holders for infromation you're going to pass to shifting function
byte data;
byte dataArray1[9];
byte dataArray2[9];


// The settings we load and save.
SettingsType settings;


// functions
void translateIR();
void BlinkAll_LEDS(int, int);

int muteLightNo=0;
bool muteLightDir = true;


void setup() {
	time = 0;
	lastTouch = 0;



#ifdef SERIAL_MONITOR
	delay(500);
	Serial.begin(9600);
	Serial.println("Serial monitor ready.");
#endif




	//Binary notation as comment
	dataArray1[0] = 0b00000000;
	dataArray1[2] = 0b00000001;
	dataArray1[1] = 0b00000010;
	dataArray1[3] = 0b00000100;
	dataArray1[4] = 0b00001000;
	dataArray1[5] = 0b00010000;
	dataArray1[6] = 0b00100000;
	dataArray1[7] = 0b01000000;
	dataArray1[8] = 0b10000000;
	



	loadSettings(); // We need to load the settings before setting up.


	Wire.begin();
	Wire.beginTransmission(PT2314_I2C);
	Wire.write(0x40); //select 1
	Wire.write(0x0); //select 1
	Wire.write(0xE0); //select 1
	Wire.endTransmission();


	//set pins to output because they are addressed in the main loop
	pinMode(latchPin, OUTPUT);
	pinMode(dataPin, OUTPUT);
	pinMode(clockPin, OUTPUT);

	pinMode(PIN_REL_POWER, OUTPUT);
	pinMode(PIN_REL_SPEAKERS, OUTPUT);

	pinMode(status_LED_pin, OUTPUT);


	digitalWrite(PIN_REL_SPEAKERS, HIGH);
	digitalWrite(PIN_REL_POWER, HIGH);


	irrecv.enableIRIn(); // Start the receiver


#ifdef SERIAL_MONITOR

	//init PT2314
	Serial.println("Starting PT2413....");
#endif	

	//load startup setting
	//audio.setSource(0, true, 3);

	audio.setSource(settings.source);
	audio.setVolume(settings.currentVolume);         // 0..63
	volume = settings.currentVolume / 4.2;
	Serial.println(settings.currentVolume);
	audio.setBass(settings.bass);      // -7..7
	audio.setTreble(settings.treble);    // -7..7
	audio.setBalance(0);   // -31..31


	//timer.setInterval(30000, repeatMe);


	Clear_LED();

}
void loop() {

	// reset LED and turn them off after 5 sec. by flashing

	if (lastTouch != 0) {
		time = millis();
		if (lastTouch + 5000 <= time) {
			lastTouch = 0;
			Clear_LED();
		}

	}


	

	if (irrecv.decode(&results)) {

		Serial.println("....");
		translateIR();

		irrecv.resume(); // Receive the next value

	}




	if (Mute) {


		if (muteLightDir){
			muteLightNo++;
			if (muteLightNo >= 16) {
				muteLightDir = false;
			}

		}
		else {
			muteLightNo--;
			if (muteLightNo <= 0) {
				muteLightDir = true;
			}
		}
		
		
		digitalWrite(latchPin, 0);
		int j = muteLightNo / 9;
		int i = muteLightNo % 9;


		if (j == 0) {
			shiftOut(dataPin, clockPin, dataArray1[i]);

			shiftOut(dataPin, clockPin, 0);
		}
		else {
			shiftOut(dataPin, clockPin, 0);

			shiftOut(dataPin, clockPin, dataArray1[i]);
		}
		//    Serial.println( pow(2, p));
		digitalWrite(latchPin, 1);
	}
	delay(30);

}


// a function to be executed periodically
void repeatMe() {

	if (IsDeviceON && IsLEDsON) {


		BlinkAll_LEDS(1, 300);
		IsLEDsON = false;
	}
}

void loadSettings() {

	// // Try to load the settings.
	byte EEPROMaddress = SETTINGS_ADDRESS;
	byte* pointer = (byte*)(void*)&settings;
	for (unsigned int i = 0; i < sizeof(settings); i++) *pointer++ = EEPROM.read(EEPROMaddress++);

	// Not ID'd as Arduino Electronic Tuner (AET) Settings?
	if (strcmp(settings.id, SETTINGS_ID_STRING) != 0) {

		// Use the default settings...
		strcpy(settings.id, SETTINGS_ID_STRING);
		settings.standby = false;
		settings.source = SOURCE_BLUETOOTH;
		//settings.displayMode        = DISPLAY_MODE_LONG;
		//settings.graphStyle         = GRAPH_STYLE_BARS;
		//settings.currentPreset      = 2;
		//settings.presetFrequency[0] = 909;
		//settings.presetFrequency[1] = 925;
		//settings.presetFrequency[2] = 977;
		//settings.presetFrequency[3] = 1029;
		//settings.presetFrequency[4] = 1053;
		//settings.presetFrequency[5] = 1077;
		//settings.currentFrequency   = 977;
		settings.currentVolume = 7;
		settings.bass = 0;
		settings.treble = 0;
		//settings.balance            = 0;
		//settings.brightness         = 7;
		//settings.random             = false;
		//settings.repeatFolder       = false;

		// Clear the "Last Track" path.
		EEPROMaddress = SETTINGS_ADDRESS + sizeof(settings);
		EEPROM.write(EEPROMaddress, 0);
	}
}

void saveSettings() {
	byte EEPROMaddress = SETTINGS_ADDRESS;
	const byte* pointer = (const byte*)(const void*)&settings;
	for (unsigned int i = 0; i < sizeof(settings); i++) EEPROM.write(EEPROMaddress++, *pointer++);
}

void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
	// This shifts 8 bits out MSB first,
	//on the rising edge of the clock,
	//clock idles low

	//internal function setup
	int i = 0;
	int pinState;
	pinMode(myClockPin, OUTPUT);
	pinMode(myDataPin, OUTPUT);

	//clear everything out just in case to
	//prepare shift register for bit shifting
	digitalWrite(myDataPin, 0);
	digitalWrite(myClockPin, 0);

	//for each bit in the byte myDataOut�
	//NOTICE THAT WE ARE COUNTING DOWN in our for loop
	//This means that %00000001 or "1" will go through such
	//that it will be pin Q0 that lights.
	for (i = 7; i >= 0; i--) {
		digitalWrite(myClockPin, 0);

		//if the value passed to myDataOut and a bitmask result
		// true then... so if we are at i=6 and our value is
		// %11010100 it would the code compares it to %01000000
		// and proceeds to set pinState to 1.
		if (myDataOut & (1 << i)) {
			pinState = 1;
		}
		else {
			pinState = 0;
		}

		//Sets the pin to HIGH or LOW depending on pinState
		digitalWrite(myDataPin, pinState);
		//register shifts bits on upstroke of clock pin
		digitalWrite(myClockPin, 1);
		//zero the data pin after shift to prevent bleed through
		digitalWrite(myDataPin, 0);
	}

	//stop shifting
	digitalWrite(myClockPin, 0);
}


void translateIR() // takes action based on IR code received describing Car MP3 IR codes
{
	if (results.value == 0xE318261B) {
		Serial.println(" Power            ");
		Power();


	}

	if (IsDeviceON) {
		switch (results.value) {

		case 0x511DBB:
			Serial.println(" Mode             ");
			break;
		case 0xEE886D7F:
			Serial.println(" Mute           ");
			if (Mute) {
				audio.setVolume(settings.currentVolume);
				Mute = false;
				Progress_LED(volume);

			}
			else {
				audio.setVolume(0);
				Mute = true;// 0..63
			}

			break;
		case 0x52A3D41F:
			Serial.println(" Play/Pause         ");
			break;
		case 0xD7E84B1B:
			Serial.println(" Previous        ");
			break;
		case 0x20FE4DBB:
			Serial.println(" Next     ");
			break;
		case 0xA3C8EDDB:
			Serial.println(" VOL-           ");

			//todo:
			/*
			read volume from pin 8 of controler
			and show current state
			*/


			if (Mute) {
				audio.setVolume(settings.currentVolume);
				Mute = false;

			}
			else {
				if (volume > 0) {
					volume--;
					settings.currentVolume = volume *4.2;
					audio.setVolume(settings.currentVolume);         // 0..63
				}
			}
			Progress_LED(volume);

			break;
		case 0xE5CFBD7F:
			Serial.println(" VOL+           ");
			if (Mute) {
				audio.setVolume(settings.currentVolume);
				Mute = false;
			}
			else {
				if (volume < 14) {
					volume++;
					settings.currentVolume = volume *4.2;
					audio.setVolume(settings.currentVolume);
					
				}
			}
			Progress_LED(volume);
			break;
			//        case 0xF076C13B:
			//          Serial.println(" EQ             ");
			//          break;
			//        case 0xC101E57B:
			//          Serial.println(" 0              ");
			//          break;
			//        case 0x97483BFB:
			//          Serial.println(" RPT         ");
			//          break;
			//        case 0xF0C41643:
			//          Serial.println(" CLOCK           ");
			//          break;
			//        case 0x9716BE3F:
			//          Serial.println(" 1              ");
			//          break;
			//
		case 0x3D9AE3F7:
			Serial.println(" Bass -              ");
			if (settings.bass > -7) {
				settings.bass -= 1;
				audio.setBass(settings.bass);
				

			}
			Progress_LED(settings.bass + 8);
			
			break;
			//
		case 0x6182021B:
			Serial.println(" Bass +             ");
			if (settings.bass < 7) {
				settings.bass += 1;
				audio.setBass(settings.bass);
			

			}
			Progress_LED(settings.bass + 8);
			
			break;
			//
			//        case 0x8C22657B:
			//        Serial.println(" 4              ");
			//        break;
			//
		case 0x488F3CBB:
			Serial.println(" Treble -              ");
			if (settings.treble > -7) {
				settings.treble -= 1;
				audio.setTreble(settings.treble);
				
			}
			Progress_LED(settings.treble + 8);
			break;
			//
		case 0x449E79F:
			Serial.println(" Treble +              ");
			if (settings.treble < 7) {
				settings.treble += 1;
				audio.setTreble(settings.treble);
				
			}
			Progress_LED(settings.treble + 8);
			break;
			
			//
			//case 0x32C6FDF7:
			//       Serial.println(" 7              ");


			//       break;
			//
			//        case 0x1BC0157B:
			//          Serial.println(" 8              ");
			//          break;
			//
			//        case 0x3EC3FC1B:
			//          Serial.println(" 9              ");
			//          break;

		default:
			Serial.print(" unknown button   ");
			Serial.println(results.value, HEX);

		}

		
	}
}




void Power() {


	if (IsDeviceON) {
		//turn off

		digitalWrite(PIN_REL_SPEAKERS, HIGH);
		Serial.println("Speaker Relay OFF.");
		delay(2000);
		digitalWrite(PIN_REL_POWER, HIGH);
		Serial.println("Power Relay OFF.");
		// If we're in standby mode, we shut down a few things.
		//bMute = false;
		//bSeeking = false;
		//bDisplayingMessage = false;
		//bytInputMode = INPUT_MODE_NORMAL;

		// Quiet please!
		audio.setVolume(0); // setVolume(map(0, MIN_VOLUME_ENC, MAX_VOLUME_ENC, MIN_VOLUME_PT2314, MAX_VOLUME_PT2314));

		// Tuner
		//if (settings.source == SOURCE_TUNER) tuner.setVolume(0);

		// VMusic3
		//if (settings.source == SOURCE_USB) {
		//	if (vmusic.mode == VMUSIC_MODE_PLAYING) {
		//		vmusic.pause();
		//		bResume = true;
		//	}
		//}

		IsDeviceON = false;
		saveSettings();
		BlinkAll_LEDS(1, 500);

	}
	else {
		digitalWrite(PIN_REL_POWER, LOW);
		Serial.println("Power Relay ON.");
		BlinkAll_LEDS(2, 200);
		delay(1000);
		digitalWrite(PIN_REL_SPEAKERS, LOW);
		Serial.println("Speaker Relay ON.");
		BlinkAll_LEDS(1, 500);

		// turn the source back on.
		//initSource();
		audio.setVolume(settings.currentVolume);//  setVolume(map(settings.currentVolume, MIN_VOLUME_ENC, MAX_VOLUME_ENC, MIN_VOLUME_PT2314, MAX_VOLUME_PT2314));
		IsDeviceON = true;
	}

}





void BlinkAll_LEDS(int n, int d) {
	//Clear_LED();

	delay(d);
	for (int x = 0; x < n; x++) {
		digitalWrite(latchPin, 0);
		shiftOut(dataPin, clockPin, 255);
		shiftOut(dataPin, clockPin, 255);
		digitalWrite(latchPin, 1);

		digitalWrite(status_LED_pin, LOW);

		delay(d);
		digitalWrite(latchPin, 0);
		shiftOut(dataPin, clockPin, 0);
		shiftOut(dataPin, clockPin, 0);
		digitalWrite(latchPin, 1);
		digitalWrite(status_LED_pin, HIGH);


		delay(d);

	}
}

void Clear_LED() {
	// clear
	digitalWrite(latchPin, 0);
	shiftOut(dataPin, clockPin, 0);
	shiftOut(dataPin, clockPin, 0);
	digitalWrite(latchPin, 1);

	digitalWrite(status_LED_pin, LOW);

}
void Progress_LED(int p) {

	Clear_LED;
	lastTouch = millis();
	digitalWrite(latchPin, 0);
	if (p <= 8) {

		shiftOut(dataPin, clockPin, pow(2, p));
		shiftOut(dataPin, clockPin, 0);
		//    Serial.println( pow(2, p));
	}
	else {
		shiftOut(dataPin, clockPin, 255);
		shiftOut(dataPin, clockPin, pow(2, p - 8));
		//   Serial.println( pow(2, p - 8));
	}

	digitalWrite(latchPin, 1);

	IsLEDsON = true;
	lastTouch = millis();
	/* todo:
	has to be remove after final release
	*/

	digitalWrite(status_LED_pin, HIGH);


}


