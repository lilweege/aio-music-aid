/*
  Description: LUQ MetroTuner Pro - All-In-One music aid/practice tool: Arduino code with serial communication
    Features include: I2S speaker playback, analog microphone frequency detection, I2C LCD tuner display, PWM LED tuner output, SD .wav file playback, and waveform generation
  Course Code: TER4M
  Submitted by: Luigi Quattrociocchi & Umberto Quattrociocchi
  Submitted to: Mr. Kolch
  Date Submitted: January 19th, 2020
*/
#include <Wire.h>
#include <SD.h>
#include <ArduinoSound.h>
#include <LiquidCrystal_I2C.h>
#include <AudioFrequencyMeter.h>

LiquidCrystal_I2C lcd =  LiquidCrystal_I2C(0x27, 16, 2);
AudioFrequencyMeter meter;                            //Microphone detection
SDWaveFile strongClick;                               //Metronome strong and weak click .wav files
SDWaveFile weakClick;

#include "Adafruit_ZeroI2S.h"
#define SAMPLERATE_HZ 44100
#define AMPLITUDE ((1<<29)-1)
#define WAV_SIZE 256
#define DRONE_GAP_TIME 0.009
#define TUNER_CALIBRATION  1.002
int32_t sine[WAV_SIZE]     = {0};
int32_t sawtooth[WAV_SIZE] = {0};
int32_t triangle[WAV_SIZE] = {0};
int32_t square[WAV_SIZE]   = {0};
Adafruit_ZeroI2S i2s;


//LED variables
int pins[] = {8, 7, 6, 1, 10};
int numLeds = 5;
int value = 255;
double led = 0;
double bright = 0;                                    //Brightness of LED

//Global variables
String latest = "";                                   //String recieved through serial communication
double f[106];
int hz = 440;

//Metronome variables
unsigned long timer;
bool mState = false;
int bpm = 90;
int mVol = 50;
double mint = 0;                                      //Temp value for float math
float mInterval = 666666.67;                          //Interval of time (s) between each metronome click
int bPattern = 4;                                     //Pattern of strong and weak clicks (1 strong per 4 clicks default)
int beat = 0;                                         //Current beat of the beat pattern

//Tuning variables
bool tState = false;
double pitch = 443; //TEST VALUE
bool tAccidentals = 0;                                //Toggle between sharps and flats: 0 for sharps, 1 for flats
String tNote = "A";
int tOctave = 4;
int pTarget;                                          //pitch number of closest pitch in tuner (A4 = 69)
double fTarget;                                       //frequency of pTarget
double cents;

//Drone variables
bool dState = false;
int dWave = 1;                                        //Drone wave types: 0 = Saw, 1 = Sine, 2 = Square, 3 = Triangle
int dVol = 50;
int drone = 60;
double dFreq = 261.63;
String dNote = "C";
int dOctave = 4;

//Generation of waveforms used in drone
void generateSine(int32_t amplitude, int32_t* buffer, uint16_t length) {
  for (int i = 0; i < length; ++i) {
    buffer[i] = int32_t(float(amplitude) * sin(2.0 * PI * (1.0 / length) * i));
  }
}
void generateSawtooth(int32_t amplitude, int32_t* buffer, uint16_t length) {
  float delta = float(amplitude) / float(length);
  for (int i = 0; i < length; ++i) {
    buffer[i] = -(amplitude / 2) + delta * i;
  }
}
void generateTriangle(int32_t amplitude, int32_t* buffer, uint16_t length) {
  float delta = float(amplitude) / float(length);
  for (int i = 0; i < length / 2; ++i) {
    buffer[i] = -(amplitude / 2) + delta * i;
  }
  for (int i = length / 2; i < length; ++i) {
    buffer[i] = (amplitude / 2) - delta * (i - length / 2);
  }
}
void generateSquare(int32_t amplitude, int32_t* buffer, uint16_t length) {
  for (int i = 0; i < length / 2; ++i) {
    buffer[i] = -(amplitude / 2);
  }
  for (int i = length / 2; i < length; ++i) {
    buffer[i] = (amplitude / 2);
  }
}

void setup() {
  Serial.begin(115200);                               //Serial communication initalized (higher rate for compatibility with MKR Zero)
  Serial.setTimeout(9);
  
  lcd.init();                                         //LCD initialized
  lcd.backlight();
  lcd.print("Booting...");
  lcd.setCursor(0,1);

  
  if (!i2s.begin(I2S_32_BIT, SAMPLERATE_HZ)) {        //I2S transmitter initialized
    lcd.print("I2S Failed!");
    while (1);
  }
  
  i2s.enableTx();

  //Generate waves
  generateSine(AMPLITUDE, sine, WAV_SIZE);
  generateSawtooth(AMPLITUDE, sawtooth, WAV_SIZE);
  generateTriangle(AMPLITUDE, triangle, WAV_SIZE);
  generateSquare(AMPLITUDE, square, WAV_SIZE);
  
  SD.begin();                                         //SD communication and .wav files initialized
  

  
  for (int i = 0; i < 5; i++) {                       //LEDs initialized
    pinMode(pins[i], OUTPUT);
  }
  calculate();                                        //Call frequency calculation function
  meter.setBandwidth(27.00, 2094.00);                 //Initializes microphone to detect pitch between A0 and C7

  lcd.print("Setting up.");
  while (Serial.readString() != "Serial Ready") ;     //Waits for Pi to connect via serial
  lcd.clear();
  lcd.print("Setup done.");
}


void calculate() {                                    //Calculates frequencies from C-1 to A7. Every value p corresponds to one semitone, starting at C-1 (69 is A4)
  float base = 2;
  float exponent;
  float exponent2;
  double multiplier;
  for (int p = 0; p < 106; p++) {
    exponent = (p - 69);
    exponent2 = exponent / 12;
    multiplier = pow(base, exponent2);
    f[p] = hz * multiplier;
  }
}

void findDronePitch() {                               //Matches the inputted drone note + octave to a note and calculates frequency of the note specified
  int semitone;
  if (dNote.equals("C"))                                semitone = 0;
  else if (dNote.equals("C#") || dNote.equals("Db"))    semitone = 1;
  else if (dNote.equals("D"))                           semitone = 2;
  else if (dNote.equals("D#") || dNote.equals("Eb"))    semitone = 3;
  else if (dNote.equals("E"))                           semitone = 4;
  else if (dNote.equals("F"))                           semitone = 5;
  else if (dNote.equals("F#") || dNote.equals("Gb"))    semitone = 6;
  else if (dNote.equals("G"))                           semitone = 7;
  else if (dNote.equals("G#") || dNote.equals("Ab"))    semitone = 8;
  else if (dNote.equals("A"))                           semitone = 9;
  else if (dNote.equals("A#") || dNote.equals("Bb"))    semitone = 10;
  else if (dNote.equals("B"))                           semitone = 11;
  dFreq = f[semitone + (dOctave + 1) * 12] * TUNER_CALIBRATION;
}

void playWave(int32_t* buffer, uint16_t length, float frequency, float seconds) {
  uint32_t iterations = seconds * SAMPLERATE_HZ;
  float delta = (frequency * length) / float(SAMPLERATE_HZ);
  for (int i = 0; i < iterations; ++i) {
    uint16_t pos = uint32_t(i * delta) % length;
    int32_t sample = buffer[pos] / 100 * dVol;
    i2s.write(sample, sample);

  }
}


void loop()
{
  latest = Serial.readString();                       //Takes the input from serial communication with the Pi
  if (latest != "") {                                 //Checks output code sent from touchscreen interface
    lcd.clear();
    if (latest.startsWith("B")) {                     //BPM and beat pattern set in metronome panel
      if (latest.startsWith("B1/")) {
        latest.replace("B1/", "");
        bPattern = latest.toInt();
        beat = 0;
        lcd.print("Beat pattern: ");
        lcd.print(latest);
      }
      else {
        latest.replace("B", "");
        bpm = latest.toInt();
        mint = 60000000.0 / bpm;
        mInterval = float(mint);
      }
    }
    else if (latest.startsWith("M")) {                //Metronome start and stop
      latest.replace("M", "");
      if (latest.equals("ON")) {
        strongClick = SDWaveFile("MSTRONG2.wav");
        weakClick = SDWaveFile("MWEAK2.wav");
        AudioOutI2S.volume(50);
        mState = true;
        tState = false;
        dState = false;
        lcd.print("Metronome ON");
      }
      else if (latest.equals("OFF")) {
        mState = false;
        lcd.print("Metronome OFF");
      }
      else {                                          //Metronome volume set by slider
        if (mVol > 100){
          mVol = 50;
          lcd.print("Error: Move");
          lcd.setCursor(0, 1);
          lcd.print("slider slower.");
        }
        mVol = latest.toInt();
        AudioOutI2S.volume(mVol);
      }
    }
    else if (latest.startsWith("H")) {                //Master frequency (hz) set in tuner panel
      latest.replace("H", "");
      hz = latest.toInt();
      calculate();
      findDronePitch();
    }
    else if (latest.startsWith("N")) {                //Drone note set in drone panel
      latest.replace("N", "");
      dNote = latest;
      findDronePitch();
    }
    else if (latest.startsWith("O")) {                //Drone octave set in drone panel
      latest.replace("O", "");
      dOctave = latest.toInt();
      findDronePitch();
    }
    else if (latest.startsWith("D")) {                //Drone start and stop
      latest.replace("D", "");
      if (latest.equals("ON")) {
        dState = true;
        tState = false;
        mState = false;
        findDronePitch();
        lcd.print("Drone playing: ");
        lcd.setCursor(0, 1);
        lcd.print(dNote);
        lcd.print(dOctave);
//        lcd.setCursor(0, 1); To display frequency, uncomment this
//        lcd.print(dFreq);
//        lcd.print(" Hz");
      }
      else if (latest.equals("OFF")) {
        dState = false;
        lcd.print("Drone OFF");
      }
      else {                                          //Drone volume set by slider
        if (dVol > 100){
          dVol = 50;
          lcd.print("Error: Move");
          lcd.setCursor(0, 1);
          lcd.print("slider slower.");
        }
        dVol = latest.toInt();
      }
    }
    else if (latest.startsWith("W")) {
      latest.replace("W", "");
      if (latest.equals("Saw")) {
        dWave = 0;
      }
      else if (latest.equals("Sine")) {
        dWave = 1;
      }
      else if (latest.equals("Square")) {
        dWave = 2;
      }
      else if (latest.equals("Triangle")) {
        dWave = 3;
      }
      findDronePitch();
    }
    else if (latest.startsWith("T")) {                //Tuner sharps/flats toggle, start, and stop
      if (latest.equals("TSHARPS")) {
        tAccidentals = 0;
      }
      else if (latest.equals("TFLATS")) {
        tAccidentals = 1;
      }
      else {
        latest.replace("T", "");
        if (latest.equals("ON")) {
          tState = true;
          dState = false;
          mState = false;
          meter.begin(A0, 45000);                     //Start pitch detection
        }
        else if (latest.equals("OFF")) {
          tState = false;
          for (int i = 0; i < 5; i++) {
            analogWrite(pins[i], 0);
          }
          meter.end();
          lcd.print("Tuner OFF");
        }
      }
    }
  }

  
  if (mState == true) {                               //Play metronome clicks
    timer = micros();
    if ((fmod(timer, mInterval) < 10000) && (fmod(timer, mInterval) > -10000)) {
      if (beat == 0) {
        AudioOutI2S.play(strongClick);
      }
      else {
        AudioOutI2S.play(weakClick);
      }
      beat++;
      if (beat == bPattern) {
        beat = 0;
      }
    }
  }

  if (tState == true) {                               //Pitch detection and tuner output
    pTarget = round(69 + 12 * (log10(pitch / hz) / log10 (2))); //Calculation to take the input pitch and find the closest pitch
    fTarget = f[pTarget];
    cents = 3986.4460836 * log10(pitch / fTarget);
    int n = (int)pTarget % 12;
    tOctave = (((int)(floor)(pTarget / 12))) - 1;
    bool natural = (n == 0 || n == 2 || n == 4 || n == 5 || n == 7 || n == 9 || n == 11);
    if (natural) {                                    //If the note is a non-accidental
      switch (n) {
        case 0: tNote = "C"; break;
        case 2: tNote = "D"; break;
        case 4: tNote = "E"; break;
        case 5: tNote = "F"; break;
        case 7: tNote = "G"; break;
        case 9: tNote = "A"; break;
        case 11: tNote = "B"; break;
        default: break;
      }
    }
    else {
      if (tAccidentals == 0) {
        switch (n) {
          case 1: tNote = "C#"; break;
          case 3: tNote = "D#"; break;
          case 6: tNote = "F#"; break;
          case 8: tNote = "G#"; break;
          case 10: tNote = "A#"; break;
          default: break;
        }
      }
      else if (tAccidentals == 1) {
        switch (n) {
          case 1: tNote = "Db"; break;
          case 3: tNote = "Eb"; break;
          case 6: tNote = "Gb"; break;
          case 8: tNote = "Ab"; break;
          case 10: tNote = "Bb"; break;
          default: break;
        }
      }
    } 
    int prev = pitch;
    pitch = meter.getFrequency();
    
    if (pitch > 0) {
      lcd.clear();
      lcd.print(tNote);
      lcd.print(tOctave);
      lcd.setCursor(0, 1);
      lcd.print(cents);
      led = map(cents, -50, 49, 0, 400);              //Map cents to LEDs
      bright = map(led - ((floor)(led / 100) * 100), 0, 100, 0, 255);
      for (int i = 0; i < 5; i++) {
        analogWrite(pins[i], 0);
      }
      analogWrite(pins[(int)((ceil)(led / 100))], bright);
      analogWrite(pins[(int)((floor)(led / 100))], 255 - bright);
    }
    else {
      for (int i = 0; i < 5; i++) {
        analogWrite(pins[i], 0);
      }
      pitch = prev;
      lcd.clear();
      lcd.print(tNote);
      lcd.print(tOctave);
      lcd.setCursor(0, 1);
      lcd.print("Signal Unclear.");
    }
  }
  
  //Commented lines play drone at the interval of the metronome bpm specified (optional)
  if (dState == true) {                       //Plays drone through speaker (checks for wave type)
    if (dWave == 0) {
      playWave(sawtooth, WAV_SIZE, dFreq, 1);
      //playWave(sawtooth, WAV_SIZE, dFreq, mInterval/1000000 - DRONE_GAP_TIME);
    }
    else if (dWave == 1) {
      playWave(sine, WAV_SIZE, dFreq, 1);
      //playWave(sine, WAV_SIZE, dFreq, mInterval/1000000 - DRONE_GAP_TIME);
    }
    else if (dWave == 2) {
      playWave(square, WAV_SIZE, dFreq, 1);
      //playWave(square, WAV_SIZE, dFreq, mInterval/1000000 - DRONE_GAP_TIME);
    }
    else if (dWave == 3) {
      playWave(triangle, WAV_SIZE, dFreq, 1);
      //playWave(triangle, WAV_SIZE, dFreq, mInterval/1000000 - DRONE_GAP_TIME);
    }
  }
}
