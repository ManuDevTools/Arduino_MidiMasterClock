#include <Adafruit_SSD1306.h>
#include <TimerOne.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <MIDI.h>

#include "LogoMochi.h"
#include "LogoName.h"

//OLED DEFINITIONS
#define OLED_I2C_ADDRESS 0x3C
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels

//PINS DEFINITIONS
#define LED_PIN 7           // Tempo LED
#define SYNC_OUTPUT_PIN 6   // Audio Sync Digital Pin
#define BUTTON_START 5      // Start/Stop Push Button

//MISC DEFINITIONS
#define CLOCKS_PER_BEAT 24  // MIDI Clock Ticks
#define AUDIO_SYNC 12       // Audio Sync Ticks

#define MINIMUM_BPM 20      // Minimum allowed BPM
#define MAXIMUM_BPM 300     // Maximum allowed BPM

#define BLINK_TIME 4        // LED blink time


//DECLARATIONS
volatile int blinkSyncCount = 0;    // Counter to sync to blinking LED
volatile int AudioSyncCount = 0;    // Counter to sync the audio "clicks"

long bpm;                           // Current BPM
int oldEncoderValue;                    // Previous encoder position

boolean playing = false;            // State if It is playing or stopped
Encoder encoder(2, 3);              // Rotary Encoder Pin 2,3 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

MIDI_CREATE_DEFAULT_INSTANCE();     //Forward declaration for MIDI instance.


//*******************************************************************************
//*******************************************************************************
//*******************************************************************************

void setup(void)
{
    //MIDI setup
    MIDI.begin();                     // MIDI init
    MIDI.turnThruOff();               // Disables the MIDI Thru, as I only want to connect 1 device

    //Internal EEPROM Setup
    bpm = EEPROMReadInt(0);           //Reads the last saved BPM
    if (bpm > MAXIMUM_BPM || bpm < MINIMUM_BPM)
        bpm = 120;

    //Interrupt Setup
    Timer1.initialize();
    Timer1.setPeriod(60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT);
    Timer1.attachInterrupt(sendClockPulse);

    //Pinmodes Setup
    pinMode(BUTTON_START,INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);

    //OLED Display Setup
    display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
    drawMochitechLogo();
    display.clearDisplay();
    display.setTextColor(WHITE);  
    display.setTextSize(4);
    display.setCursor(0,0);
    display.print(" " + String(bpm));
    display.display();
}


//*******************************************************************************
//*******************************************************************************
//*******************************************************************************

void loop(void)
{
    if (digitalRead(BUTTON_START) == LOW) 
    {
      startOrStop();
      delay(300);
    }

    int encoderValue = (encoder.read()/4);

    if (encoderHasChanged(encoderValue))
    {
        if (encoderHasIncreased(encoderValue))
            increaseBPM();
        else
            decreaseBPM();

        oldEncoderValue = encoderValue;
    }
}


//*******************************************************************************
//*******************************************************************************
//*******************************************************************************

//Encoder Functions
bool encoderHasChanged(int encoderValue)
{
    if (encoderValue != oldEncoderValue)
        return true;
    else
        return false;
}

bool encoderHasIncreased(int encoderValue)
{
    if (encoderValue < oldEncoderValue)
        return false;
    else
        return true;
}


//*******************************************************************************
//*******************************************************************************
//*******************************************************************************

//BPM Functions

void increaseBPM()
{
    bpm++;

    if (bpm > MAXIMUM_BPM)
        bpm = MAXIMUM_BPM;

    bpm_display();
}


void decreaseBPM()
{
    bpm--;

    if (bpm < MINIMUM_BPM)
    {
        bpm = MINIMUM_BPM;
    }

    bpm_display();
}

//*******************************************************************************
//*******************************************************************************
//*******************************************************************************

//EEPROM Functions
void EEPROMWriteInt(int p_address, int p_value)
{
    byte lowByte = ((p_value >> 0) & 0xFF);
    byte highByte = ((p_value >> 8) & 0xFF);

    EEPROM.write(p_address, lowByte);
    EEPROM.write(p_address + 1, highByte);
}

unsigned int EEPROMReadInt(int p_address)
{
    byte lowByte = EEPROM.read(p_address);
    byte highByte = EEPROM.read(p_address + 1);

    return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}


//*******************************************************************************
//*******************************************************************************
//*******************************************************************************

//Display Functions
void bpm_display()
{
    updateTimerTime();
    EEPROMWriteInt(0,bpm);
    display.clearDisplay();
    display.setCursor(0,0);

    if (bpm > 99)                         //This is to keep the text align on the right
      display.print(" " + String(bpm));
    else
      display.print("  " + String(bpm));

    display.display();
}


//*******************************************************************************
//*******************************************************************************
//*******************************************************************************

//Logic Functions
void startOrStop()
{
    if (!playing) 
    {
        MIDI.sendRealTime(midi::Start);
        AudioSyncCount = 0;
        blinkSyncCount = 0;

        MIDI.sendRealTime(midi::Clock);
        digitalWrite(SYNC_OUTPUT_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);

    } 
    else 
    {
        MIDI.sendRealTime(midi::Stop);
        digitalWrite(SYNC_OUTPUT_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
    }

    playing = !playing;
}


void updateTimerTime()
{
    long interval = 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;  
    Timer1.setPeriod(interval);
}


void sendClockPulse()
{
    if (playing) 
    {
        MIDI.sendRealTime(midi::Clock);

        blinkSyncCount = (blinkSyncCount + 1) % CLOCKS_PER_BEAT;
        AudioSyncCount = (AudioSyncCount + 1) % AUDIO_SYNC;

        if (AudioSyncCount == 0) 
            digitalWrite(SYNC_OUTPUT_PIN, HIGH); 
        else 
            digitalWrite(SYNC_OUTPUT_PIN, LOW);
        
        if (blinkSyncCount == 0)
            digitalWrite(LED_PIN, HIGH);
        else if (blinkSyncCount == BLINK_TIME)
            digitalWrite(LED_PIN, LOW);
    }
}


void drawMochitechLogo()
{
    for(int i=-36; i<0; i++)
    {
        display.clearDisplay();
        display.drawBitmap(i, 0, LogoMochi, 33, 27, WHITE);
        display.display();
    }

    for(int i=-21; i<3; i++)
    {
        display.clearDisplay();
        display.drawBitmap(0, 0, LogoMochi, 33, 27, WHITE);
        display.drawBitmap(35,i, NameOled, 91, 23, WHITE);
        display.display();
    }

    delay(2000);
}