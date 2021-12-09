/*  DaemonBite Arcade Encoder
 *  Author: Mikael Norrgård <mick@daemonbite.com>
 *
 *  Copyright (c) 2020 Mikael Norrgård <http://daemonbite.com>
 *  
 *  GNU GENERAL PUBLIC LICENSE
 *  Version 3, 29 June 2007
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *  
 *
 *  Modified for use in the ArcadeR USB
 *  Matt Barszcz 2021
 *  Version 1.0
 */

#include "Gamepad.h"
#include <EEPROM.h>			// EEPROM library to save button sync state

#define DEBOUNCE 0          // 1=Diddly-squat-Delay-Debouncing™ activated, 0=Debounce deactivated
#define DEBOUNCE_TIME 10    // Debounce time in milliseconds
//#define DEBUG             // Enables debugging (sends debug data to usb serial)
#define LONGPRESS 1000		// Define length in ms of a long press to toggle Button 1 & 2 sync

const char *gp_serial = "Daemonbite ArcadeR USB";

#define UP    0x80
#define DOWN  0x40
#define LEFT  0x20
#define RIGHT 0x10

Gamepad_ Gamepad;               // Set up USB HID gamepad
bool usbUpdate = false;         // Should gamepad data be sent to USB?
bool debounce = DEBOUNCE;       // Debounce?
uint8_t  pin;                   // Used in for loops
uint32_t millisNow = 0;         // Used for Diddly-squat-Delay-Debouncing™

uint32_t buttons78Millis = 0;   // Counter for Long Press
bool buttons78State = 0;        // State of Holding both buttons
bool buttons78LongPress = 0;    // State of Long Press
bool button1Button2Sync = 0;    // Power up Button 1&2 Sync Initial State

uint8_t  axesDirect = 0x0f;
uint8_t  axes = 0x0f;
uint8_t  axesPrev = 0x0f;
uint8_t  axesBits[4] = {0x10, 0x20, 0x40, 0x80};
uint32_t axesMillis[4];

uint16_t buttonsDirect = 0;
uint16_t buttons = 0;
uint16_t buttonsPrev = 0;
uint16_t buttonsBits[12] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800};
uint32_t buttonsMillis[12];

#ifdef DEBUG
	char buf[16];
	uint32_t millisSent = 0;
#endif

void setup(){
  // Axes
  DDRF  &= ~B11110000; // Set A0-A3 as inputs
  PORTF |=  B11110000; // Enable internal pull-up resistors

  // Buttons
  DDRD  &= ~B10011111; // Set PD0-PD4 and PD7 as inputs
  PORTD |=  B10011111; // Enable internal pull-up resistors
  DDRB  &= ~B01111110; // Set PB1-PB6 as inputs
  PORTB |=  B01111110; // Enable internal pull-up resistors

  // Debounce selector switch (currently disabled)
  DDRE  &= ~B01000000; // Pin 7 as input
  PORTE |=  B01000000; // Enable internal pull-up resistor

  // Initialize debouncing timestamps
  for (pin = 0; pin < 4; pin++)
    axesMillis[pin] = 0;
  for (pin = 0; pin < 12; pin++)
    buttonsMillis[pin] = 0;
	
  // Load Last Button 1/2 Sync state from EEPROM
  button1Button2Sync = EEPROM.read(0);

  Gamepad.reset();

#ifdef DEBUG
  Serial.begin(115200);
#endif
}

void loop(){
  // Get current time, the millis() function should take about 2µs to complete
  millisNow = millis();

  // Check debounce selector switch
  // debounce = (PINE & B01000000) ? false : true;

  for (uint8_t i = 0; i < 10; i++){
  // One iteration (when debounce is enabled) takes approximately 35µs to complete, so we don't need to check the time between every iteration
  
    // Read axis and button inputs (bitwise NOT results in a 1 when button/axis pressed)
    axesDirect = ~(PINF & B11110000);
    buttonsDirect = ~((PIND & B00011111) | ((PIND & B10000000) << 4) | ((PINB & B01111110) << 4));

    if (debounce){
      // Debounce axes
      for (pin = 0; pin < 4; pin++)
      {
        // Check if the current pin state is different to the stored state and that enough time has passed since last change
        if ((axesDirect & axesBits[pin]) != (axes & axesBits[pin]) && (millisNow - axesMillis[pin]) > DEBOUNCE_TIME)
        {
          // Toggle the pin, we can safely do this because we know the current state is different to the stored state
          axes ^= axesBits[pin];
          // Update the timestamp for the pin
          axesMillis[pin] = millisNow;
        }
      }

      // Debounce buttons
      for (pin = 0; pin < 12; pin++){
        // Check if the current pin state is different to the stored state and that enough time has passed since last change
        if ((buttonsDirect & buttonsBits[pin]) != (buttons & buttonsBits[pin]) && (millisNow - buttonsMillis[pin]) > DEBOUNCE_TIME)
        {
          // Toggle the pin, we can safely do this because we know the current state is different to the stored state
          buttons ^= buttonsBits[pin];
          // Update the timestamp for the pin
          buttonsMillis[pin] = millisNow;
        }
      }
    }
    else {
      axes = axesDirect;
      buttons = buttonsDirect;
    }

	//If you are holding both buttons for the first time
	if ( (buttons & B0000011 << 6) ) {					// If both buttons 7 & 8 are pressed
		if (buttons78State == 0){						// but weren't previously
			buttons78State = 1;							// Set State
			buttons78LongPress = 1;						// Begin a long press event
			buttons78Millis = millisNow;				// record millis at start of long press
		}else{ 	//Nth time through the loop with the buttons pressed			
			if (((millisNow - buttons78Millis) > LONGPRESS)){   // Check to see if buttons have  been held long enough
				button1Button2Sync = !button1Button2Sync;		// flip button 1/2 sync state
				EEPROM.put(0,button1Button2Sync);				// Save state to EEPROM
				flashRxLED();                         			// Flash RX LED to let you know it happened
			}
		}
	}else {								
		buttons78LongPress = 0;		// If buttons aren't being pressed anymore, its no longer a long press
		buttons78State = 0; 		// Unset state
	}

    // Have axis inputs changed?
    if (axes != axesPrev){

      // UP + DOWN = UP, SOCD (Simultaneous Opposite Cardinal Directions) Cleaner
      if (axes & B10000000)
        Gamepad._GamepadReport.Y = -1;
      else if (axes & B01000000)
        Gamepad._GamepadReport.Y = 1;
      else
        Gamepad._GamepadReport.Y = 0;
      // UP + DOWN = NEUTRAL
      //Gamepad._GamepadReport.Y = ((axes & B01000000)>>6) - ((axes & B10000000)>>7);
      // LEFT + RIGHT = NEUTRAL
      Gamepad._GamepadReport.X = ((axes & B00010000) >> 4) - ((axes & B00100000) >> 5);

      axesPrev = axes;
      usbUpdate = true;
    }

    // Have button inputs changed?
    if (buttons != buttonsPrev){
		
		// Are buttons 1 and 2 synced?
		if (button1Button2Sync == 1){
			if ( buttons & B00000001 << 1) {		//If button 2 is pressed  
				buttons = (buttons & ~B00000010);	//Negate Button 2 Press
				buttons = (buttons | B00000001);	//Replace with Button 1 Press 
			}
		}

      Gamepad._GamepadReport.buttons = buttons;

      buttonsPrev = buttons;
      usbUpdate = true;
    }

    // Should gamepad data be sent to USB?
    if (usbUpdate){
		Gamepad.send();
		usbUpdate = false;

		#ifdef DEBUG
			  //Serial.println(buttons,BIN);
			  sprintf(buf, "%06lu: %d%d%d%d", millisNow - millisSent, ((axes & 0x10) >> 4), ((axes & 0x20) >> 5), ((axes & 0x40) >> 6), ((axes & 0x80) >> 7) );
			  Serial.print(buf);
			  sprintf(buf, " %d%d%d%d", (buttons & 0x01), ((buttons & 0x02) >> 1), ((buttons & 0x04) >> 2), ((buttons & 0x08) >> 3) );
			  Serial.println(buf);
			  millisSent = millisNow;
		#endif
    }
  }
}

void flashRxLED(){
    for(int i=0; i<=10; i++){
      digitalWrite(17,LOW);
      delay(40);
      digitalWrite(17,HIGH);
      delay(40);
    }
}
