
/*
  Code stolen / adapted from here: http://www.instructables.com/id/8X8X8-RGB-LED-Cube/step7/DOCUMENTATION-and-Firmware-for-Programming-and-con/

  This currently is designed to control an 8x8x8 cube, controlling 192 RGB leds individually and then having a mosfet to cycle through each of the 8 layers.
  Essentially controlling 200 outputs total
  We need it to control more leds, but we only have 4 "layers" and we need to control 12 leds at a time per tile. We currently have 32 tiles, so we need to control 512 outputs,
  but more if we expand the floor.

  For a prototype, we just need to control 16: 12 leds and 4 "layers" The bit that's going to be difficult is this guy wrote this at a hardware level.
  The weird all caps variables are registers on the arduino, and he uses that to make sure the clock speed is high enough, instead of going through the SPI library.

  Also important to note, this essentially bit-bangs the outputs at different rates, creating PWM signals that vary brightness, that's why the clock speed is so high and
  the code is so complicated. We may have to have two arduinos to run all of the tiles, but we'll see if we can do it with one, maybe reducing the resolution of the PWM.
*/

#include <SPI.h>// SPI Library used to clock data out to the shift registers

#define latch_pin 2// can use any pin you want to latch the shift registers
#define blank_pin 4// same, can use any pin you want for this, just make sure you pull up via a 1k to 5V
#define data_pin 11// used by SPI, 51 on mega
#define clock_pin 13// used by SPI, 52 on mega

//***variables***variables***variables***variables***variables***variables***variables***variables
//These variables are used by multiplexing and Bit Angle Modulation Code
int shift_out;//used in the code a lot in for(i= type loops
byte anode[4] = {
  0b00001110,
  0b00001101,
  0b00001011,
  0b00000111
};

int activeRow = 0;

const int tileRows = 4;
const int tileCols = 4;
const int totalTiles = tileRows * tileCols;

//This is how the brightness for every LED is stored,
//Each LED only needs a 'bit' to know if it should be ON or OFF, so 64 Bytes gives you 512 bits= 512 LEDs
//Since we are modulating the LEDs, using 4 bit resolution, each color has 4 arrays containing 64 bits each
//Only 4 bits of each byte contain the information to be written
byte red01[totalTiles * 4], red2[totalTiles * 4];
byte green01[totalTiles * 4], green2[totalTiles * 4];
byte blue01[totalTiles * 4], blue2[totalTiles * 4];
//notice how more resolution will eat up more of your precious RAM

int anodelevel = 0; //this increments through the anode levels
int BAM_Bit, BAM_Counter = 0; // Bit Angle Modulation variables to keep track of things

//These variables can be used for other things
unsigned long start;//for a millis timer to cycle through the animations

//****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup
void setup() {

  SPI.setBitOrder(MSBFIRST);//Most Significant Bit First
  SPI.setDataMode(SPI_MODE0);// Mode 0 Rising edge of data, keep clock low
  SPI.setClockDivider(SPI_CLOCK_DIV2);//Run the data in at 16MHz/2 - 8MHz

  //Serial.begin(115200);// if you need it?
  noInterrupts();// kill interrupts until everybody is set up

  //We use Timer 1 to refresh the cube
  TCCR1A = B00000000;//Register A all 0's since we're not toggling any pins
  TCCR1B = B00001011;//bit 3 set to place in CTC mode, will call an interrupt on a counter match
  //bits 0 and 1 are set to divide the clock by 64, so 16MHz/64=250kHz
  TIMSK1 = B00000010;//bit 1 set to call the interrupt on an OCR1A match
  OCR1A = 30; // you can play with this, but I set it to 30, which means:
  //our clock runs at 250kHz, which is 1/250kHz = 4us
  //with OCR1A set to 30, this means the interrupt will be called every (30+1)x4us=124us,
  // which gives a multiplex frequency of about 8kHz

  //finally set up the Outputs
  pinMode(latch_pin, OUTPUT);//Latch
  pinMode(data_pin, OUTPUT);//MOSI DATA
  pinMode(clock_pin, OUTPUT);//SPI Clock
  //pinMode(blank_pin, OUTPUT);//Output Enable  important to do this last, so LEDs do not flash on boot up
  SPI.begin();//start up the SPI library
  interrupts();//let the show begin, this lets the multiplexing start

  Serial.begin(9600);

}//***end setup***end setup***end setup***end setup***end setup***end setup***end setup***end setup***end setup***end setup


void loop() { //***start loop***start loop***start loop***start loop***start loop***start loop***start loop***start loop***start loop

  //Each animation located in a sub routine
  // To control an LED, you simply:
  // LED(level you want 0-7, row you want 0-7, column you want 0-7, red brighness 0-15, green brighness 0-15, blue brighness 0-15);




  //testFade();
  allOn();
  while(true) delay(1000);



}//***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop



void LED(int row, int column, byte red, byte green, byte blue) { //****LED Routine****LED Routine****LED Routine****LED Routine
  //This is where it all starts
  //This routine is how LEDs are updated, with the inputs for the LED location and its R G and B brightness levels

  // First, check and make sure nothing went beyond the limits, just clamp things at either 0 or 7 for location, and 0 or 15 for brightness

  //  if (row < 0)
  //    row = 0;
  //  if (row > tileRows<<2)
  //    row = tileRows * 4;
  //  if (column < 0)
  //    column = 0;
  //  if (column > tileCols<<2)
  //    column = tileCols * 4;
  //  if (red < 0)
  //    red = 0;
  //  if (red > 15)
  //    red = 15;
  //  if (green < 0)
  //    green = 0;
  //  if (green > 15)
  //    green = 15;
  //  if (blue < 0)
  //    blue = 0;
  //  if (blue > 15)
  //    blue = 15;


  //There are 512 LEDs in the floor, that needs to be translated into a number from 0 to 511
  //The way I did indexing matches how the individual bytes are written in the final design. The first row of each tile represents the first 1/4 of the indexes, the next row the next 1/4 and so on.
  //from there I used the ternary operator to allow the indices to zigzag across the dancefloor with the tiles.
  int whichbyte = int(((column & 0x07) < 4) ? (row & 0x03) * totalTiles + (column >> 3) * (tileRows << 1) + (row >> 2) : (row & 0x03) * totalTiles + ((column >> 3) + 1) * (tileRows << 1) - (row >> 2) - 1);
  int whichbit = int(column & 0x03);

  //  Serial.print(row);
  //  Serial.print(", ");
  //  Serial.print(column);
  //  Serial.print(", ");
  //  Serial.println(whichbyte);


  //These arrays contain the 4 bits of the color resolution. Each byte contains 4 bits of useful information. red and blue contain it in the most significant
  //4 bits. Green in the least. This is because they will be bitwise OR'd together later to send the two byte package to each tile in the form R,G,B,row
  bitWrite(red01[whichbyte], whichbit, bitRead(red, 0));
  bitWrite(red01[whichbyte], whichbit + 4, bitRead(red, 1));
  bitWrite(red2[whichbyte], whichbit, bitRead(red, 2));

  bitWrite(green01[whichbyte], whichbit, bitRead(green, 0));
  bitWrite(green01[whichbyte], whichbit + 4, bitRead(green, 1));
  bitWrite(green2[whichbyte], whichbit, bitRead(green, 2));

  bitWrite(blue01[whichbyte], whichbit, bitRead(blue, 0));
  bitWrite(blue01[whichbyte], whichbit + 4, bitRead(blue, 1));
  bitWrite(blue2[whichbyte], whichbit, bitRead(blue, 2));

}//****LED routine end****LED routine end****LED routine end****LED routine end****LED routine end****LED routine end****LED routine end

ISR(TIMER1_COMPA_vect) { //***MultiPlex BAM***MultiPlex BAM***MultiPlex BAM***MultiPlex BAM***MultiPlex BAM***MultiPlex BAM***MultiPlex BAM

  //This routine is called in the background automatically at frequency set by OCR1A
  //In this code, I set OCR1A to 30, so this is called every 124us, giving each level in the cube 124us of ON time
  //There are 8 levels, so we have a maximum brightness of 1/8, since the level must turn off before the next level is turned on
  //The frequency of the multiplexing is then 124us*8=992us, or 1/992us= about 1kHz


  PORTD |= 1 << blank_pin; //The first thing we do is turn all of the LEDs OFF, by writing a 1 to the blank pin
  //Note, in my bread-boarded version, I was able to move this way down in the cube, meaning that the OFF time was minimized
  //do to signal integrity and parasitic capcitance, my rise/fall times, required all of the LEDs to first turn off, before updating
  //otherwise you get a ghosting effect on the previous level

  //This is 4 bit 'Bit angle Modulation' or BAM, There are 4 rows, so when a '1' is written to the color brightness,
  //each level will have a chance to light up for 1 cycle, the BAM bit keeps track of which bit we are modulating out of the 4 bits
  //Bam counter is the cycle count, meaning as we light up each level, we increment the BAM_Counter
  if (BAM_Counter == 4)
    BAM_Bit++;
  else if (BAM_Counter == 12)
    BAM_Bit++;

  BAM_Counter++;//Here is where we increment the BAM counter

  switch (BAM_Bit) { //The BAM bit will be a value from 0-3, and only shift out the arrays corresponding to that bit, 0-3
    //Here's how this works, each case is the bit in the Bit angle modulation from 0-4,
    case 0:
      for (shift_out = activeRow; shift_out < activeRow + totalTiles; shift_out++) {
        SPI.transfer((red01[shift_out] << 4) | (green01[shift_out] & 0x0F));
        SPI.transfer((blue01[shift_out] << 4) | anode[BAM_Counter % 4]);
      }
      break;
    case 1:
      for (shift_out = activeRow; shift_out < activeRow + totalTiles; shift_out++) {
        SPI.transfer((red01[shift_out] & 0xF0) | (green01[shift_out] >> 4));
        SPI.transfer((blue01[shift_out] & 0xF0) | anode[BAM_Counter % 4]);
      }
      break;
    case 2:
      for (shift_out = activeRow; shift_out < activeRow + totalTiles; shift_out++) {
        SPI.transfer((red2[shift_out] << 4) | (green2[shift_out] & 0x0F));
        SPI.transfer((blue2[shift_out] << 4) | anode[BAM_Counter % 4]);
      }
      //Here is where the BAM_Counter is reset back to 0, it's only 4 bit, but since each cycle takes 3 counts,
      if (BAM_Counter == 28) {
        BAM_Counter = 0;
        BAM_Bit = 0;
      }
      break;
  }//switch_case

  PORTD |= 1 << latch_pin; //Latch pin HIGH
  PORTD &= ~(1 << latch_pin); //Latch pin LOW
  PORTD &= ~(1 << blank_pin); //Blank pin LOW to turn on the LEDs with the new data

  activeRow = activeRow + totalTiles;

  if (activeRow >= totalTiles * 4) //if you hit 64 on level, this means you just sent out all 63 bytes, so go back
    activeRow = 0;
  pinMode(blank_pin, OUTPUT);//moved down here so outputs are all off until the first call of this function
}//***MultiPlex BAM END***MultiPlex BAM END***MultiPlex BAM END***MultiPlex BAM END***MultiPlex BAM END***MultiPlex BAM END***MultiPlex BAM END



//*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE
//*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE
//*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE
//*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE*+*+*+*+*+*+*+*+*+*+*+*+PUT ANIMATIONS DOWN HERE

void testFade() {
  int xxx = 0, yyy = 0, red = 7, green = 7, blue = 7, start = 0;
  int delRed = -1, delGreen = 0, delBlue = 0;
  while (true) {
    if (millis() > start + 1000) {
      start = millis();
      if (red == 0 && delRed == -1) delRed = 1;
      if (red == 7 && delRed == 1) {
        delRed = 0;
        delGreen = -1;
      }
      if (green == 0 && delGreen == -1) delGreen = 1;
      if (green == 7 && delGreen == 1) {
        delGreen = 0;
        delBlue = -1;
      }
      if (blue == 0 && delBlue == -1) delBlue = 1;
      if (blue == 7 && delBlue == 1) {
        delBlue = 0;
        delRed = -1;
      }
      red += delRed;
      green += delGreen;
      blue += delBlue;

      for (xxx = 0; xxx < tileRows * 4; xxx++) {
        for (yyy = 0; yyy < tileCols * 4; yyy++) {
          LED(xxx, yyy, red, green, blue);
        }
      }

    }
  }
}

void allOn() {
  for (int xxx = 0; xxx < tileRows * 4; xxx++) {
    for (int yyy = 0; yyy < tileCols * 4; yyy++) {
      LED(xxx, yyy, 7, 4, 2);
    }
  }
}



