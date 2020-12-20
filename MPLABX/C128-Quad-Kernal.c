/*
 * File:   C128-Quad-Kernal.c
 * Author: Barry Walker (barrywalker@gmail.com)
 *
 * Based on code created by discoHR
 * https://github.com/discoHR/C64C-C128-multikernal-adapter
 * 
 * Created on December 5, 2020, 10:04 PM
 */

#pragma config FOSC = INTRCIO   // Oscillator Selection bits (INTOSC oscillator: I/O function on GP4/OSC2/CLKOUT pin, I/O function on GP5/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-Up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = OFF      // GP3/MCLR pin function select (GP3/MCLR pin function is digital I/O, MCLR internally tied to VDD)
#pragma config BOREN = OFF      // Brown-out Detect Enable bit (BOD disabled)
#pragma config CP = OFF         // Code Protection bit (Program Memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)

#define _XTAL_FREQ  4000000

#include <xc.h>
#include <pic12f629.h>

// inputs
#define RESTORE_N   GPIObits.GP3

// outputs
#define A14         GPIObits.GP5
#define A15         GPIObits.GP4
#define RED_LED1    GPIObits.GP1
#define RED_LED2    GPIObits.GP0

// input/outputs
#define RESET_N     GPIObits.GP2

// EEPROM addresses used for configuration persistence
#define EEPROM_ADDR_KERNAL 0

#define ON      1
#define OFF     0
#define ALL_ON  3

typedef unsigned char byte;
typedef unsigned short word;

enum state_t {
    IDLE, SELECT
} main_state = IDLE;

enum select_t {
    INITIALIZING, SELECTING, SELECTED
} select_state = INITIALIZING;

byte kernalIndex    = 0;
byte ignoreReset    = 0;
byte pressedTime    = 0;

/*
 * Hacky way to get a variable delay
 */
void Delay(word ms) {
    for(int i = 0; i < ms; i++) {
        __delay_ms(1);
    }
}

/*
 * Return 1 if the RESTORE key is pressed
 */
byte RestorePressed() {
    byte pressed = 0;
    if(!RESTORE_N) {
        Delay(10);
        if(!RESTORE_N) {
            pressed = 1;
        }
    }
    return pressed;
}

/*
 * Return the amount of time that the RESTORE key was held down with a max value.
 * Used to distinguish short and long presses.
 */
byte RestorePressedTime(byte max)
{
    byte count = 0;
    while(RestorePressed() && count < max) {
        count++;
    }
    return count;
}

/*
 * Set A14 and A15 to match the selected kernal
 */
void SetKernal(byte value) {
    A14 = value & 01 ? 1 : 0;
    A15 = value & 02 ? 1 : 0;
    Delay(20);
}

/*
 Set our 2 red LEDs to a value between 0 and 3
 */
void SetLED(byte value) {
    RED_LED1 = value & 01 ? 1 : 0;
    RED_LED2 = value & 02 ? 1 : 0;
}

/*
 * Write the currently selected Kernal to EEPROM
 */
void SaveKernal(byte index) {
    EEPROM_WRITE(EEPROM_ADDR_KERNAL, index);
}

/*
 * Load the selected Kernal from EEPROM
 */
byte LoadKernal() {
    return EEPROM_READ(EEPROM_ADDR_KERNAL);
}

/*
 * Turn both LEDs off
 */
void LEDsOff()
{
    SetLED(OFF);
}

/*
 * Turn both LEDs on
 */
void LEDsOn()
{
    SetLED(ALL_ON);
}

/*
 * Turn the LEDs off, pause for x ms and then turn the back on
 */
void FlashLEDs(byte value, word delay) {
    LEDsOff();
    Delay(delay);
    SetLED(value);
}

/*
 * During power on, flash the LED with the pattern showing the currently selected
 * Kernals.
 */
void FlashSelectedKernal()
{
    for(int i = 0; i < 5; i++) {
        FlashLEDs(kernalIndex, 500);
        Delay(500);
    }
}

void DoReset(void) {
    RESET_N = 0;
    TRISIObits.TRISIO2 = 0; // pull INTRES_N low
    FlashLEDs(ALL_ON, 50);
    Delay(200);
    TRISIObits.TRISIO2 = 1; // release INTRES_N
    Delay(250); // possible fix for double-reset cartridges
}

void Init(void) {
    OPTION_REG      = 0;
    WPUbits.WPU1    = 1;
    WPUbits.WPU2    = 1;
    WPUbits.WPU5    = 1;
    CMCON           = 0b00000111; // digital IO
    TRISIO          = 0b00001100;
    RESET_N         = 1;
    RESTORE_N       = 1;
    RED_LED1        = 1;
    RED_LED2        = 1;
    
    Delay(50);
    
    kernalIndex = LoadKernal();
    SetKernal(kernalIndex);
    DoReset();

    IOC     = 0b00001100;       // GPIO interrupt-on-change for both inputs
    GPIE    = 1;                // GPIO Interrupt enable, on
    GIE     = 0;                // Global interrupt enable, off

    FlashSelectedKernal();

    LEDsOn();
    
    Delay(250); // ignore reset during power up
}

void main(void) {
    Init();

    while(1) {
        switch(main_state) {
            case IDLE:   
                // Look for a long press while in our idle loop. That's how
                // we get into select mode. If no long press, sleep the processor.
                if(RestorePressedTime(200) == 200) {
                    FlashLEDs(ON, 100);
                    while(RestorePressed()) {}
                    main_state = SELECT;
                } else {
                    SLEEP();
                    GPIF = 0;
                }
                break;
                        
            case SELECT:
                switch(select_state) {
                    case INITIALIZING:
                        // Load the currently selected kernal from EEPROM and
                        // show it on the 128's LEDs
                        kernalIndex = LoadKernal();
                        SetLED(kernalIndex);
                        select_state = SELECTING;
                        
                    case SELECTING:
                        // Here we can either get a long or a short press. A long
                        // press saves the currently selected kernal to EEPROM and
                        // resets the machine. A short press cycles through the
                        // kernals while displaying the selected kernal on the 128's
                        // LEDs
                        pressedTime = RestorePressedTime(200);
                        switch(pressedTime) {
                            case 200:
                                FlashLEDs(ON, 100);
                                select_state = SELECTED;
                                break;
                            case 1 ... 100:
                                while(RestorePressed()) {}
                                kernalIndex++;
                                if(kernalIndex == 4) kernalIndex = 0;
                                SetLED(kernalIndex);
                                break;
                            default:
                                SLEEP();
                                GPIF = 0;
                        }
                        break;
                        
                    case SELECTED:
                        // Save the selected kernal to EEPROM, configure A14/A15
                        // to point at it and reset the machine.
                        main_state = IDLE;
                        select_state = INITIALIZING;
                        SaveKernal(kernalIndex);
                        SetKernal(kernalIndex);
                        DoReset();
                        break;
                }               
                break;
        }
    }
}
