/*
   C128 Quad Kernal

   attiny84 sketch for the C128 Quad Kernal adapter which allows 4 different kernals using the RESTORE key to
   cycle through them. Can be paired with an additional adapter for the U32 C64 BASIC/Kernal to allow 4 separate
   kernals for the C64 portion of a 128 as well.

   Copyright © 2021 Barry Walker

*/

#include <avr/io.h>
#include <EEPROM.h>
#include <AceButton.h>

using namespace ace_button;

const int KERNAL_COUNT      = 4;

// On boot, we flash the selected kernal 5 times on the 128's LEDs
const int FLASH_COUNT       = 5;
const int LONG_FLASH_DELAY  = 250;
const int SHORT_FLASH_DELAY = 100;
const int RESET_DELAY       = 400;

// Default to 2 seconds for a long button press
const int LONG_PRESS_DELAY  = 2000;

const int BOTH_LEDS_ON      = 3;
const int BOTH_LEDS_OFF     = 0;

// Output Pins
const int LED1_PIN          = 1;  // PIN 13
const int LED2_PIN          = 0;  // PIN 12

const int A14_PIN           = 10; // PIN 2
const int A15_PIN           = 9;  // PIN 3

// Input Pins
const int RESTORE_PIN       = 3;  // PIN 10
const int RESET_PIN         = 2;  // PIN 11

// We save the selected kernal in byte 0 of EEPROM
int CURRENT_KERNAL_ID       = 0;

// We save the selected mode in byte 1 of EEPROM
int CURRENT_MODE_ID         = 1;

int currentKernal = 0;

// Keeps track of the state we're in
enum State {
  IDLE,
  SELECT
} state = IDLE;

// Keeps track of the mode we're in. There are 2 modes:
// BRIEF will only show the current kernal on the LEDs
// when the kernal is changed, but keep both LEDs lit the
// rest of the time.
// DETAILED will show the current kernal on the LEDs at
// all times - including when kernal 00 is selected which
// will result in both LEDs being off.
// To change modes, hold down the RESTORE key when you turn
// the machine on. 
enum Mode {
  BRIEF,
  DETAILED
} mode = DETAILED;

AceButton restore(RESTORE_PIN);

void handleEvent(AceButton*, uint8_t, uint8_t);

void setup() {
  mode = loadMode();
  
  // LEDs and address lines are OUTPUTs
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(A14_PIN, OUTPUT);
  pinMode(A15_PIN, OUTPUT);

  // Configure RESTORE and RESET for INPUTs
  // Both lines are active low
  pinMode(RESET_PIN, INPUT);
  pinMode(RESTORE_PIN, INPUT_PULLUP);
  digitalWrite(RESET_PIN, HIGH);

  // If RESTORE is held down at boot, change modes. The change is indicated
  // by quickly alternating the status of the LEDs.
  if(digitalRead(RESTORE_PIN) == 0)
  {
    alternateLEDs(10,50);
    changeMode();
    saveMode(mode);
  }

  restore.getButtonConfig()->setFeature(ButtonConfig::kFeatureLongPress);
  restore.getButtonConfig()->setFeature(ButtonConfig::kFeatureDoubleClick);
  restore.getButtonConfig()->setLongPressDelay(LONG_PRESS_DELAY);
  restore.setEventHandler(handleEvent);

  currentKernal = loadKernal();
  doReset();
}

void loop() {
  restore.check();
}

/*
 * Handle the pressing of the RESTORE key
 */
void handleEvent(AceButton* /*button*/, uint8_t eventType,
                 uint8_t /*buttonState*/) {
  bool longPressed = eventType == AceButton::kEventLongPressed;
  bool clicked = eventType == AceButton::kEventClicked;
  bool doubleClicked = eventType == AceButton::kEventDoubleClicked;

  switch(state) {
    case IDLE:
      if(longPressed) {
        state = SELECT;
        flickerLEDs();
        currentKernal = loadKernal();
        updateLEDs(currentKernal);
      }
      if(doubleClicked) {
        currentKernal = bumpKernal(currentKernal);
        doReset();
      }
      break;

    case SELECT:
      if(longPressed) {
        state = IDLE;
        flickerLEDs();
        doReset();
      }
      if(clicked) {
        currentKernal = bumpKernal(currentKernal);
        updateLEDs(currentKernal);
      }
      break;
  }
}

/*
 * Increment the current kernal
 */
int bumpKernal(int value)
{
  value++;
  if(value == KERNAL_COUNT) value = 0;
  return value;
}

/*
 * Load the saved kernal from EEPROM
 */
int loadKernal()
{
  return loadFromEEPROM(CURRENT_KERNAL_ID);
}

/*
 * Save the current kernal to EEPROM
 */
void saveKernal(int value)
{
  saveToEEPROM(CURRENT_KERNAL_ID, value);
}

/*
 * Retrieve the current mode from EEPROM
 */
int loadMode()
{
  return loadFromEEPROM(CURRENT_MODE_ID);
}

/*
 * Save the current mode to EEPROM
 */
void saveMode(int value)
{
  saveToEEPROM(CURRENT_MODE_ID, value);
}

/*
 * Generic routine to load data from EEPROM
 */
int loadFromEEPROM(int location)
{
  return EEPROM.read(location);
}

/*
 * Generic routine to save data to EEPROM
 */
void saveToEEPROM(int location, int value)
{
  EEPROM.write(location, value);
}

/*
 * Configure A14/A15 to point at the selected kernal
 */
void updateAddressLines(int value)
{
  digitalWrite(A14_PIN, value & 1);
  digitalWrite(A15_PIN, value & 2);
}

/*
 * Update the 128's LEDs to show a value from 0 - 3
 */
void updateLEDs(int value)
{
  digitalWrite(LED1_PIN, value & 1);
  digitalWrite(LED2_PIN, value & 2);
}

/*
 * Flash the 128's LEDs with a value from 0 - 3
 */
void flashLEDs(int value, int count, int ms)
{
  for(int i = 0; i < count; i++)
  {
    blinkLEDs(value, ms);
    delay(ms);
  }
  updateLEDs(BOTH_LEDS_ON);
}

/*
 * Flash the 128's LEDs one time with a value and a delay time
 */
void blinkLEDs(int value, int ms)
{
  updateLEDs(BOTH_LEDS_OFF);
  delay(ms);
  updateLEDs(value);
}

/*
 * Indicate the current kernal on the LEDs by flashing the binary value
 */
void flashCurrentKernal()
{
  flashLEDs(currentKernal, FLASH_COUNT, LONG_FLASH_DELAY);
}

/*
 * Do a quick flicker of the LEDs to show that selection has been accepted
 */
void flickerLEDs()
{
  blinkLEDs(BOTH_LEDS_ON, SHORT_FLASH_DELAY);
}

/*
 * Alternate the LEDs between digital 01 and 02 which just alternates the
 * flashing of the LEDs
 */
void alternateLEDs(int count, int ms)
{
  for(int i = 0; i < count; i++)
  {
    updateLEDs(1);
    delay(ms);
    updateLEDs(2);
    delay(ms);
  }
}

/*
 * Save the kernal to EEPROM, update the address lines and reset the machine
 */
void doReset()
{
  saveKernal(currentKernal);
  updateAddressLines(currentKernal);
  
  digitalWrite(RESET_PIN, LOW);
  pinMode(RESET_PIN, OUTPUT);
  
  delay(RESET_DELAY);
  
  pinMode(RESET_PIN, INPUT);
  digitalWrite(RESET_PIN, HIGH);

  delay(RESET_DELAY);
  
  if(mode == BRIEF)
  {
    flashCurrentKernal();
    updateLEDs(BOTH_LEDS_ON);
  } else {
    updateLEDs(currentKernal);
  }
}

void changeMode()
{
  mode = mode == BRIEF ? DETAILED : BRIEF;
}
