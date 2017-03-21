
/*
  Code adapted from here: http://www.instructables.com/id/8X8X8-RGB-LED-Cube/step7/DOCUMENTATION-and-Firmware-for-Programming-and-con/

  This Program is for an rgb dancefloor made of 4x4 tiles and hooked up tile to tile, starting in the top left, proceding downwards,
  then zig-zagging right. It controls the RGB values with 3-bit resolution. You can use 4, but youre limited to ~25 tiles. At 3 bit,
  you can control ~55.

  Colors are set by calling the LED() method. PWM is handled in the background, so you are able to use blocking statements like delay()
  if you wish

  TODO: Get to work on the Mega
  TODO: Reduce memory footprint
  TODO: Mess with SPI speed
*/

#include <SPI.h>// SPI Library used to clock data out to the shift registers

#define latch_pin 2// can use any pin you want to latch the shift registers
#define blank_pin 4// same, can use any pin you want for this, just make sure you pull up via a 1k to 5V
#define data_pin 11// used by SPI, 51 on mega
#define clock_pin 13// used by SPI, 52 on mega

//***variables***variables***variables***variables***variables***variables***variables***variables
//These variables are used by multiplexing and Bit Angle Modulation Code
int shift_out; //index of which bits to shift out
byte anode[4] = { //anodes for multiplexing
  0b00001000,
  0b00000001, //FIXME Hackish
  0b00000010,
  0b00000100
};

int activeRow = 0;

const int tileRows = 1; //Rows and Columns in the dancefloor
const int tileCols = 1;
const int totalTiles = tileRows * tileCols;


byte red01[totalTiles * 4], red2[totalTiles * 4]; //Arrays that store the bits for how bright each Led should be
byte green01[totalTiles * 4], green2[totalTiles * 4];
byte blue01[totalTiles * 4], blue2[totalTiles * 4];
//notice how more resolution will eat up more of your precious RAM

int anodelevel = 0; //this increments through the anode levels
int BAM_Bit, BAM_Counter = 0; // Bit Angle Modulation variables to keep track of the cycle in the pwm

int k = 0;

//These variables can be used for other things
unsigned long start;//for a millis timer to cycle through the animations

//****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup****setup
void setup() {

  SPI.setBitOrder(LSBFIRST);//Least Significant Bit First
  SPI.setDataMode(SPI_MODE0);// Mode 0 Rising edge of data, keep clock low
  SPI.setClockDivider(SPI_CLOCK_DIV2);//Run the data in at 16MHz/2 - 8MHz

  //Serial.begin(115200);// if you need it?
  noInterrupts();// kill interrupts until everybody is set up

  //We use Timer 1 to refresh the cube
  TCCR1A = B00000000;//Register A all 0's since we're not toggling any pins
  TCCR1B = B00001011;//bit 3 set to place in CTC mode, will call an interrupt on a counter match
  //bits 0 and 1 are set to divide the clock by 64, so 16MHz/64=250kHz
  TIMSK1 = B00000010;//bit 1 set to call the interrupt on an OCR1A match
  OCR1A = 140; // play with this to increase or reduce cycle rate
  //with OCR1A set to 30, this means the interrupt will be called every (30+1)x4us=124us,
  // which gives a multiplex frequency of about 8kHz

  //finally set up the Outputs
  pinMode(latch_pin, OUTPUT);//Latch
  pinMode(data_pin, OUTPUT);//MOSI DATA
  pinMode(clock_pin, OUTPUT);//SPI Clock
  //pinMode(blank_pin, OUTPUT);//Output Enable  important to do this last, so LEDs do not flash on boot up
  SPI.begin();//start up the SPI library
  interrupts();//let the show begin, this lets the multiplexing start

  //Serial.begin(9600);

}//***end setup***end setup***end setup***end setup***end setup***end setup***end setup***end setup***end setup***end setup


void loop() { //***start loop***start loop***start loop***start loop***start loop***start loop***start loop***start loop***start loop

  //Each animation located in a sub routine
  // To control an LED, you simply:
  // LED(row you want, column you want, red brighness 0-7, green brighness 0-7, blue brighness 0-7);




  //testFade();
  //testRow();
    //allOn();
    testMulti();



}//***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop***end loop



void LED(int row, int column, byte red, byte green, byte blue) { //****LED Routine****LED Routine****LED Routine****LED Routine
  //This is where it all starts
  //This routine is how LEDs are updated, with the inputs for the LED location and its R G and B brightness levels

  // Commented out for speed. may not matter

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


  //Best line of code ever
  //Takes the row and column input and indexes them to how we are going to write them to the floor
  //I wäs having speed issues so I used a ternary operator, masks and shifts but essentially it says:
  //if(column%4 < 4) whichbyte = row%4 * totalTiles + column*tileRows/4 + row/4;
  //else whichbyte = row%4 * totalTiles + (column+8)*tileRows/4 - row/4 - 1;
  int whichbyte = int(((column & 0x07) < 4)
                      ? (row & 0x03) * totalTiles + (column >> 3)*(tileRows << 1) + (row >> 2)
                      : (row & 0x03) * totalTiles + ((column >> 3) + 1)*(tileRows << 1) - (row >> 2) - 1);
  int whichbit = int(column & 0x03);



  //These arrays contain the 3 bits of the color resolution. bits 0&1 of each led for a row within a tile
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
  //By increasing OCR1A to 140, the LEDs flicker more, but it allows this method to be called slow enough for us drive all the data we need
  //out to all the tiles without them dimming noticeably

  PORTD |= 1 << blank_pin; //The first thing we do is turn all of the LEDs OFF, by writing a 1 to the blank pin
  //Note, in my bread-boarded version, I was able to move this way down in the cube, meaning that the OFF time was minimized
  //do to signal integrity and parasitic capcitance, my rise/fall times, required all of the LEDs to first turn off, before updating
  //otherwise you get a ghosting effect on the previous level

  //This is 3 bit 'Bit angle Modulation' or BAM, There are 4 rows, so when a '1' is written to the color brightness,
  //each level will have a chance to light up for 1 cycle, the BAM bit keeps track of which bit we are modulating out of the 4 bits
  //Bam counter is the cycle count, meaning as we light up each row, we increment the BAM_Counter
  if (BAM_Counter == 4)
    BAM_Bit++;
  else if (BAM_Counter == 12)
    BAM_Bit++;

  BAM_Counter++;//Here is where we increment the BAM counter

  switch (BAM_Bit) { //The BAM bit will be a value from 0-3, and only shift out the arrays corresponding to that bit, 0-3
    //Here's how this works, each case is the bit in the Bit angle modulation from 0-4,
    case 0:
      for (shift_out = activeRow; shift_out < activeRow + totalTiles; shift_out++) {
        SPI.transfer((blue01[shift_out] << 4) | anode[BAM_Counter & 0x03]);
        SPI.transfer((red01[shift_out] << 4) | (green01[shift_out] & 0x0F));
      }
      break;
    case 1:
      for (shift_out = activeRow; shift_out < activeRow + totalTiles; shift_out++) {
        SPI.transfer((blue01[shift_out] & 0xF0) | anode[BAM_Counter % 4]);
        SPI.transfer((red01[shift_out] & 0xF0) | (green01[shift_out] >> 4));
      }
      break;
    case 2:
      for (shift_out = activeRow; shift_out < activeRow + totalTiles; shift_out++) {
        SPI.transfer((blue2[shift_out] << 4) | anode[BAM_Counter % 4]);
        SPI.transfer((red2[shift_out] << 4) | (green2[shift_out] & 0x0F));
      }
      //Here is where the BAM_Counter is reset back to 0, it's only 3 bit, but since each cycle takes 4counts,
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
  int xxx = 0, yyy = 0, red = 7, green = 7, blue = 7;
  int delRed = -1, delGreen = 0, delBlue = 0;
  start = 0;
  while (true) {
    if (millis() >= start + 50) {
      start = millis();
      if (red <= 0 && delRed == -1) delRed = 1;
      if (red >= 7 && delRed == 1) {
        delRed = 0;
        delGreen = -1;
      }
      if (green <= 0 && delGreen == -1) delGreen = 1;
      if (green >= 7 && delGreen == 1) {
        delGreen = 0;
        delBlue = -1;
      }
      if (blue <= 0 && delBlue == -1) delBlue = 1;
      if (blue >= 7 && delBlue == 1) {
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
void testRow() {
  int xxx = 0, yyy = 0, red = 7, green = 0, blue = 0;
  while (true) {
    for (yyy = 0; yyy < 4; yyy++) {
      for (xxx = 0; xxx < 4; xxx++) {
        LED(xxx, yyy, red, green, blue);
      }
      delay(1000);
      for (xxx = 0; xxx < 4; xxx++) {
        LED(xxx, yyy, 0, 0, 0);
      }
    }
    for (xxx = 0; xxx < 4; xxx++) {
      for (yyy = 0; yyy < 4; yyy++) {
        LED(xxx, yyy, red, green, blue);
      }
      delay(1000);
      for (yyy = 0; yyy < 4; yyy++) {
        LED(xxx, yyy, 0, 0, 0);
      }
    }
  }
}

void testMulti(){
//  for(int i=0; i<4; i++){
//    LED(i,i,7,7,7);
//  }
  LED(0,0,7,7,7);
  delay(1000);
}

void allOn() {
  for (int xxx = 0; xxx < tileRows * 4; xxx++) {
    for (int yyy = 0; yyy < tileCols * 4; yyy++) {
      LED(xxx, yyy, 0, 0, 7);
    }
  }
}



