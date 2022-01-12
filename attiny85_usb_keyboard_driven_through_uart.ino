/*
  A generic USB keyboard device, taking commands of what keys to send via uart.
  
  The use case is creating a hardware keyboard with 
  single hardware buttons that then emit complex key combinations
  that control an application (either directly when the app is focused
  or through global hotkeys, see below).

  Target: Attiny85 microcontroller
  Dependencies: 
    - Arduino
    - Digispark's boards into the manager, and the DigiKeyboard, see https://github.com/digistump/DigistumpArduino
    - SoftSerial_INT0, see https://github.com/J-Rios/Digispark_SoftSerial-INT0
    - [the device providing the commands via serial]
  Tests: see the marked code sections  
  Author:  VARADI NAGY Pal
  Author's site: csillagtura.ro
  License: MIT, as-is
  
  Disclaimer: tested with OBS (Open Broadcaster Software) in Windows10, 
  and the global hotkeys in OBS work if sent as a sequence ('s')
  but not as a single strike ('S'), see below

*/


#include <SoftSerial_INT0.h>
#include "DigiKeyboard.h"

#define INSIDE_ARDUINO__SO_NO_TESTS


#define PIN_RX 2              
#define PIN_TX 1              
#define SOFTSERIAL_BAUDRATE 9600

#define DELAY_MS_BEFORE_ENDING_SEQUENCE_WITH_KEYRELEASE 50
#define DELAY_MS_AFTER_ADDING_MODIFIER_BIT_IN_SEQUENCE 90

  
SoftSerial softSerial(PIN_RX, PIN_TX);

char incomingStringLine[16];
    // message format
    
    // #
    // S(trike) (press+release)
    // KK key as hexascii
    // MM modifier as hexascii
    // character 13 and/or 10
    
    // #  
    // s(trike) same as above, but 
    //    the scancode plus modifiers 
    //    are built up sequentially    
    //    instead of getting sent 
    //    in one single keystroke
    
    // #
    // e(cho)
    // C(haracter to be echoed)
    // character 13 and/or 10
    
byte constructedModifierMask;

// TEST BEGIN: run this block in an online compiler

#define BITS_TO_AND_WITH_TO_OBTAIN_UPPERCASE_LETTERS (255-(1 << 5))
#define TO_UPPERCASE_ISH(X) (X & BITS_TO_AND_WITH_TO_OBTAIN_UPPERCASE_LETTERS)
#define IS_ENTER(X) ( ((X)==13) || ((X)==10) )

int digitToInt(unsigned char c){
  if ((c >= 48)&&(c < 48+10)){
    return c-48;
  }
  c = TO_UPPERCASE_ISH(c);
  if ((c >=65)&&(c<65+6)){
    return c-65+10;
  }
  return 0;
}

int twoHexAsciiDigitsToInt(const char * d){  
  //strtol is too heavy, given the modest flash size
  return digitToInt(d[0])*16 + digitToInt(d[1]); 
}

#ifndef INSIDE_ARDUINO__SO_NO_TESTS

void test_twoHexAsciiDigitsToInt(void){
  printf("Testing twoHexAsciiDigitsToInt(char * d0d1)\n");
  int err = 0;
  if (twoHexAsciiDigitsToInt("00") != 0x00) err++;
  if (twoHexAsciiDigitsToInt("01") != 0x01) err++;
  if (twoHexAsciiDigitsToInt("10") != 0x10) err++;
  if (twoHexAsciiDigitsToInt("11") != 0x11) err++;
  if (twoHexAsciiDigitsToInt("A1") != 0xA1) err++;
  if (twoHexAsciiDigitsToInt("AA") != 0xAA) err++;
  if (twoHexAsciiDigitsToInt("AB") != 0xAB) err++;
  if (twoHexAsciiDigitsToInt("FF") != 0xFF) err++;
  if (twoHexAsciiDigitsToInt("ab") != 0xAB) err++;
  if (twoHexAsciiDigitsToInt("ff") != 0xFF) err++;
  if (twoHexAsciiDigitsToInt("f1") != 0xF1) err++;
  if (err > 0){
    printf("FAILED. Total errors: %d.\n", err);
  }else{
    printf("PASSED. No errors.");
  }
  
}

int main(){
  test_twoHexAsciiDigitsToInt();
  return 0;
}
 
#endif


// TEST END: run this block in an online compiler


static inline void constructedModifierMask_check(byte finalMask, byte partialMask){  
    if (0 != (partialMask & finalMask)){
      constructedModifierMask |= partialMask;
      DigiKeyboard.sendKeyPress(0, constructedModifierMask);
      DigiKeyboard.delay(DELAY_MS_AFTER_ADDING_MODIFIER_BIT_IN_SEQUENCE);
    }
}  

static inline void sendKeyPlusModifierAsStrokeOrSequence(void){
  byte k = twoHexAsciiDigitsToInt(incomingStringLine+2);
  byte m = twoHexAsciiDigitsToInt(incomingStringLine+4);
  if ('s' == incomingStringLine[1]){
    constructedModifierMask = 0;
    for (int j=0; j<8; j++){
      constructedModifierMask_check(m, (1 << j));
    }
  }  
  DigiKeyboard.sendKeyPress(k, m);
  if ('s' == incomingStringLine[1]){
    DigiKeyboard.delay(DELAY_MS_BEFORE_ENDING_SEQUENCE_WITH_KEYRELEASE);
    DigiKeyboard.sendKeyStroke(0);
  }
}


static inline void digestIncomingStringLine(void){ 
  if ('#' != incomingStringLine[0]){
    return ;
  }
  DigiKeyboard.sendKeyStroke(0);//digispark's recomm from example code
  if ('e' == incomingStringLine[1]){    
    incomingStringLine[3] = 0;
    DigiKeyboard.print(incomingStringLine+2);    
    return ;
  }
  if ('S' == TO_UPPERCASE_ISH(incomingStringLine[1]) ){ 
    // check for both upper- and lowercase S
    sendKeyPlusModifierAsStrokeOrSequence();
  }
}

void setup() {
  // put your setup code here, to run once:
  softSerial.begin(SOFTSERIAL_BAUDRATE);  
  DigiKeyboard.sendKeyStroke(0);
}


void loop() {
  if (softSerial.available()){
    byte c = softSerial.read();
    if ('#' == c){
      incomingStringLine[0] = 0;
    }
    int lengo = strlen(incomingStringLine);
    if (lengo < sizeof(incomingStringLine)-2){
      incomingStringLine[lengo++] = c;
      incomingStringLine[lengo] = 0;
    }
    if (IS_ENTER(c)){
      digestIncomingStringLine();
      incomingStringLine[0] = 0;
    }
  }
}

