/* IR Remote Wand v2 - see http://www.technoblogy.com/show?25TN

   David Johnson-Davies - www.technoblogy.com - 13th May 2018
   ATtiny85 @ 1 MHz (internal oscillator; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <avr/sleep.h>

// IR transmitter **********************************************

// Buttons

const int S1 = 4;
const int S2 = 3;
const int S3 = 5;  // Reset
const int S4 = 2;
const int S5 = 0;
const int LED = 1;           // IR LED output

// Pin change interrupt service routine
ISR (PCINT0_vect) {
  int in = PINB;
  if ((in & 1<<S1) == 0) Send('M', 0x0707, 0xFD02);
  else if ((in & 1<<S2) == 0) Send('M', 0x0707, 0xFB04);
  else if ((in & 1<<S4) == 0) Send('R', 0x0013, 0x0011);
  else if ((in & 1<<S5) == 0) Send('R', 0x0013, 0x0010);
}

const int top = 25;                       // 1000000/26 = 38.5kHz
const int match = 19;                     // approx. 25% mark/space ratio

// Set up Timer/Counter1 to output PCM on OC1A (PB1)
void SetupPCM () {
  TCCR1 = 1<<PWM1A | 3<<COM1A0 | 1<<CS10; // Inverted PWM output on OC1A divide by 1
  OCR1C = top;                            // 38.5kHz
  OCR1A = top;                            // Keep output low  
}

// Generate count cycles of carrier followed by gap cycles of gap
void Pulse (int count, int gap) {
  OCR1A = match;                          // Generate pulses  
  for (int i=0; i<2; i++) {
    for (int c=0; c<count; c++) {
      while ((TIFR & 1<<TOV1) == 0);
      TIFR = 1<<TOV1;
    }
  count = gap;                            // Generate gap
  OCR1A = top;
  }
}

void Send (char IRtype, unsigned int Address, unsigned int Command) {
  
  // NEC or Samsung codes
  if ((IRtype == 'N') || (IRtype == 'M')) {
    unsigned long code = ((unsigned long) Command<<16 | Address);
    TCNT1 = 0;                            // Start counting from 0
    // Send Start pulse
    if (IRtype == 'N') Pulse(342, 171); else Pulse(190, 190);
    // Send 32 bits
    for (int Bit=0; Bit<32; Bit++)
      if (code & ((unsigned long) 1<<Bit)) Pulse(21, 64); else Pulse(21, 21);
    Pulse(21, 0);
  
  // Sony 12, 15, or 20 bit codes
  } else if (IRtype == 12 || IRtype == 15 || IRtype == 20) {
    unsigned long code = ((unsigned long) Address<<7 | Command);
    TCNT1 = 0;                            // Start counting from 0
    // Send Start pulse
    Pulse(96, 24);
    // Send 12, 15, or 20 bits
    for (int Bit=0; Bit<IRtype; Bit++)
      if (code & ((unsigned long) 1<<Bit)) Pulse(48, 24); else Pulse(24, 24);
  
  // Philips RC-5 code
  } else if (IRtype == 'R') {
    static int toggle = toggle ^ 1;
    int nextbit, extended = Command>>6 ^ 1;
    unsigned int code = 0x2000 | extended<<12 |
      toggle<<11 | Address<<6 | (Command & 0x3F);
    TCNT1 = 0;                            // Start counting from 0
    for (int b=0; b<14; b++) {
      nextbit = code>>(13-b) & 1;
      for (uint8_t i=0; i<2; i++) {
        if (nextbit) OCR1A = top; else OCR1A = match;
        // Wait for 32 Timer/Counter1 overflows
        for (int c=0; c<32; c++) {
          while ((TIFR & 1<<TOV1) == 0);
          TIFR = 1<<TOV1;                 // Clear overflow flag
        }
        nextbit = !nextbit;
      }
    }
    OCR1A = top;                          // Leave output off
  }
}

// Setup demo **********************************************

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(S1, INPUT_PULLUP);
  pinMode(S2, INPUT_PULLUP);
  pinMode(S4, INPUT_PULLUP);
  pinMode(S5, INPUT_PULLUP);
  SetupPCM();
  // Configure pin change interrupts to wake on button presses
  PCMSK = 1<<S1 | 1<<S2 | 1<<S4 | 1<<S5;
  GIMSK = 1<<PCIE;                  // Enable interrupts
  GIFR = 1<<PCIF;                   // Clear interrupt flag
  // Disable what we don't need to save power
  ADCSRA &= ~(1<<ADEN);             // Disable ADC
  PRR = 1<<PRUSI | 1<<PRADC;        // Turn off clocks to unused peripherals
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  // Send S3 code on reset
  Send('R', 0x0013, 0x000C);
}

// Stay asleep and just respond to interrupts
void loop() {
  sleep_enable();
  sleep_cpu();
}
