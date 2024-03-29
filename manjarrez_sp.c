// ------- Preamble ------- //
#define __AVR_ATmega168__ 
#define BAUD 9600
#define ADXL345_ADDRESS_W 0b10100110
#define ADXL345_ADDRESS_R 0b10100111
#define bits sizeof(uint16_t)*8
#include "pinDefines.h"
#include <avr/io.h>      //Macros, ports, registers, etc.
#include <util/delay.h>  //Library for seconds   
#include <util/setbaud.h>
#include <avr/interrupt.h> //Library for interrupts
#include "lcd.h"
#include <stdlib.h>

//-----Button Functions-----//
void button_init(); //Initialize button behavior
void interrupts_init(); //Initilize interrupts
ISR(INT0_vect); //Interrupt service routine
uint8_t debounce(void); //Debouncing function for button

//-----Biking Stats-----//
void riding_bike();
void gbike_greeting();
static int button_start = 1;
int flag = 1; //Flag for checking states
int has_started=0; //Flag for checking states
int time = 0; //Total time biked
char time_to_string[4]; //itoa preface

//-----Accelerometer-----//
//i2c Functions (from Canvas)
void i2c_accelerometer();
void initI2C(void);
void i2cWaitForComplete(void);
void i2cStart(void); 
void i2cStop(void);
uint8_t i2cReadAck(void);
uint8_t i2cReadNoAck(void);
void i2cSend(uint8_t data);

//-----ADXL345 + LCD Display-----//
typedef enum terrain {Flat, Rocky, Mixed, Uphill, Downhill} terrain; //Types of terrain
int* total_average;
uint16_t average_num; 
uint16_t x, y, z;
int isNegative = 0;
static int prev[2];
int iter = 0;
int terrain_difference;
int av_terrain[30];
int counter = 0;

char* terrain_type_string;
char* average_terrain(); //Returns average terrain
char* terrain_type(); //Displays string of current detected terrain
void calculate_terrain(); //Gran x, y, z accelerometer values to useable data

int main(void) {

   //------Initializations-----//
   LCD_Init();
   button_init();
   interrupts_init();
   initI2C();
   i2c_accelerometer();

   LCD_Clear();
   LCD_String("Hi from G-Bike!");
   _delay_ms(1000);
   LCD_String_xy(1,0, "Biking companion");
   _delay_ms(3000);

   LCD_Clear();
   LCD_String("Press button to");
   LCD_String_xy(1,0, "start ride! :)");
   while(1);

    return 0; 
}

//---------------BUTTON FUNCTIONALITY---------------//

void button_init() {
    DDRD &= ~(1<<PD2); //Double checks if we are in input mode (book)
    PORTD |= (1<<PD2); //Enbles pull-up resistor
}

//The following code has been applied from SER456 Slides 18
void interrupts_init() {
    EIMSK |= (1<< INT0); //Enabling interrupt INT0
    EICRA |= (1<<ISC00); //Configuring INT0
    sei(); //Activate all enabled interrupts
}

// Interupt Service Routine - 
// Runs code that will return the last character input to the computer
// as required for full assignment credit
ISR(INT0_vect) { //Runs when there is a button change
    if(debounce()) {
       PORTB ^= (1<<PB1);

       if(has_started) { //For continuous use
          gbike_greeting();
       }

       if(flag) { //Checks different states (first, used, etc.)

          flag = 0;
          has_started = 0;
          riding_bike(); //Main communication prompt

       } else { 

          LCD_Clear();
          LCD_String("Thanks for");
          LCD_String_xy(1,0, "riding! :)");
          flag = 1;
          has_started = 1;
       }
    }
}

//SOURCE: MAKE: AVR Programming by Elliot Williams
uint8_t debounce(void) {
    if (bit_is_clear(PIND, PD2)) { //When button is pressed
        _delay_us(1000); //delay 1000 miliseconds to ignore noise
        if (bit_is_clear(PIND, PD2)) { //return "true" if button is still noise
            return 1;
        }
    }
    return 0; //break out if button is ready
}

void riding_bike() { //Main communication prompt for user
   button_start = 0;
   LCD_Clear();
   LCD_String("Ride starting in");
   LCD_String_xy(1,8, "3");
   _delay_ms(1000);
   LCD_String_xy(1,8, "2");
   _delay_ms(1000);
   LCD_String_xy(1,8, "1");
   _delay_ms(1000);
   LCD_Clear();
   LCD_String_xy(0,5,"Start!");
   _delay_ms(1000);
   gbike_main_display();
}

void gbike_greeting() { //Greets user and prompts them for button press
   LCD_Clear();
   LCD_String("Hi from G-Bike!");
   _delay_ms(1000);
   LCD_String_xy(1,0, "Biking companion");
   _delay_ms(3000);

   LCD_Clear();
   LCD_String("Press button to");
   LCD_String_xy(1,0, "start ride! :)");
   while(button_start);
}

void gbike_main_display() {
   LCD_Clear();
   LCD_String("Terrain: ");
   LCD_String_xy(1,0, "Time: ");
   LCD_String_xy(1,9,"s");
   while (time < 30) { //30 seconds
      time++;
      itoa(time, time_to_string, 10); //converts time (seconds) to char*
      calculate_terrain();
      LCD_Clear();
      LCD_String("Terrain: ");
      LCD_String_xy(1,0, "Time: ");
      LCD_String_xy(1,9,"s");
      LCD_String_xy(0,9, terrain_type());
      LCD_String_xy(1,6, time_to_string);
      _delay_ms(1000);
      counter++;
   }

   LCD_Clear();
   LCD_String("Average terrain: ");
   LCD_String_xy(1,6, average_terrain());
   time = 0; //reset
}

//-----Accelerometer/I2C Functions-----//

void i2c_accelerometer() {
    i2cStart(); //Init serial communication 
    i2cSend(ADXL345_ADDRESS_W); //Begin communication with sensor
    i2cSend(0x2D); //POWER_CTL 
    i2cSend(1 << 3); //D3 set to high (measure)
    i2cStop(); 
}

void initI2C(void) {
                                     /* set pullups for SDA, SCL lines */
  I2C_SDA_PORT |= ((1 << I2C_SDA) | (1 << I2C_SCL));
  TWBR = 32;   /* set bit rate (p.242): 8MHz / (16+2*TWBR*1) ~= 100kHz */
  TWCR |= (1 << TWEN);                                       /* enable */
}

void i2cWaitForComplete(void) {
  loop_until_bit_is_set(TWCR, TWINT);
}

void i2cStart(void) {
  TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWSTA));
  i2cWaitForComplete();
}

void i2cStop(void) {
  TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWSTO));
}

uint8_t i2cReadAck(void) {
  TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWEA));
  i2cWaitForComplete();
  return (TWDR);
}

uint8_t i2cReadNoAck(void) {
  TWCR = (_BV(TWINT) | _BV(TWEN));
  i2cWaitForComplete();
  return (TWDR);
}

void i2cSend(uint8_t data) {
  TWDR = data;
  TWCR = (_BV(TWINT) | _BV(TWEN));                  /* init and enable */
  i2cWaitForComplete();
}

//Displays string of current detected terrain
char* terrain_type() {

   if(iter==0) {
         prev[iter] = z;
         iter++;
      } else {
         terrain_difference = (prev[0]-z);
            if(terrain_difference < 0) 
               terrain_difference *= -1;
         iter = 0;
      }
   
   if((z>>15)==1) {
      z*=-1;
      if (z >= 0 && z < 30) {
         if(terrain_difference > 10) {
            terrain_type_string = "Jagged";
            av_terrain[counter] = 1;
         } else {
            terrain_type_string = "Flat";
            av_terrain[counter] = 2;
         }
      } else {
         terrain_type_string = "Incline";
         av_terrain[counter] = 3;
         return terrain_type_string;
   }
   } else { 
      terrain_type_string = "Decline";
      av_terrain[counter] = 4;
   }

//z += 2;

// if((z>>15)==1) {
//    terrain_type_string = "-";
// } else {
//    terrain_type_string = "+";
// }
// char temp[5]; 
//    itoa(z, temp, 10);
//    terrain_type_string = temp;
   return terrain_type_string;
}   


//Grab z accelerometer value to useable data
void calculate_terrain() {

      i2cStart();
      i2cSend(ADXL345_ADDRESS_W); 
      i2cSend(0x34);
      i2cStart();
      i2cSend(ADXL345_ADDRESS_R);
      z = i2cReadAck();
      z |= i2cReadNoAck() << 8;
      i2cStop();
} 

char* average_terrain() {
   int sum = 0; 
   int total = 30; 
   int average = 0;
   for(int i = 0; i < 30; i++) {
      sum+= av_terrain[i];
   }

   average = sum/total;

   if(average == 1) {
      return "Jagged";
   } else if (average == 2) {
      return "Flat";
   } else if (average == 3) {
      return "Incline";
   } else if (average == 4) {
      return "Decline";
   }

   return "No Data";
}