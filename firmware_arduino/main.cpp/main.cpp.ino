/* 
 * File:   globals.h
 * Author: robertoalcantara
 * 
 * Just for fun. Do whatever u want.
*/


#include "globals.h"
#include "leds.c"
#include <Joystick.h>

// Create the Joystick
Joystick_ Joystick;

volatile T_GLOBAL_TIMER global_timer;

void setup() {

  DDRB = B00001110; 
  DDRC = B00000000; 
  DDRD = B01110000; 
  DDRE = B00000000; 
  DDRF = B00000000; 

  //PORTF = 0; //no pullup

  
  // TIMER 1 for interrupt frequency 2000 Hz:
  cli(); // stop interrupts
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1  = 0; // initialize counter value to 0
  // set compare match register for 2000 Hz increments
  OCR1A = 7999; // = 16000000 / (1 * 2000) - 1 (must be <65536)
  TCCR1B |= (1 << WGM12);// turn on CTC mode
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10);   // Set CS12, CS11 and CS10 bits for 1 prescaler
  TIMSK1 |= (1 << OCIE1A);   // enable timer compare interrupt
  sei(); // allow interrupts


  Serial.begin(9600);
  Joystick.begin();
  led_scan_update( 21 ); // non wired led, just to cleanup any data on latches
}


/* timer1 ISR to set timer flags */
ISR(TIMER1_COMPA_vect) {    

  global_timer.flags.flag.on512us = 1;
  global_timer.flags.flag.on1ms = global_timer.aux_10ms & B00000001 ;
    
  global_timer.aux_10ms++;
    if ( global_timer.aux_10ms > 9 ) { 
    global_timer.aux_10ms = 0;
    global_timer.flags.flag.on10ms = 1;
    
    global_timer.aux_100ms++;
    if ( global_timer.aux_100ms > 9 ) {
      global_timer.aux_100ms = 0;
      global_timer.flags.flag.on100ms = 1;
      
      global_timer.aux_1s++;
      if ( global_timer.aux_1s > 9 ) {
          global_timer.aux_1s = 0;
          global_timer.flags.flag.on1s = 1;
      }        
    }
  }  
}


/* adc readings and simple filter */
#define NUM_SAMPLES 16
#define ANALOG_MULTI_QTY 5
unsigned long dev_value[ANALOG_MULTI_QTY];
void analog_tick() {
  
  static unsigned long dev_acum[ANALOG_MULTI_QTY];
  static unsigned char sample_cnt;
  static unsigned char current_device;
    
  switch ( current_device ) {
    case 0: dev_acum[ 0 ]  += analogRead(5); /*blocking ~100us SW1 */
            break;
    case 1: dev_acum[ 1 ]  += analogRead(4); /*blocking ~100us SW2*/
            break;
    case 2: dev_acum[ 2 ]  += analogRead(3); /*blocking ~100us SW3*/
            break;
    case 3: dev_acum[ 3 ]  += analogRead(2); /*blocking ~100us Btn1-7*/
            break;
    case 4: dev_acum[ 4 ]  += analogRead(0); /*blocking ~100us Btn8-12*/
            sample_cnt++;
            break;
  }  
  
  if ( current_device == ANALOG_MULTI_QTY-1) {
    current_device = 0;
  } else {
    current_device++;
  }
  
  if (sample_cnt == NUM_SAMPLES) {
    /* just do the math*/
    sample_cnt = 0;
    for (unsigned char tmp=0; tmp<ANALOG_MULTI_QTY; tmp++) {
      dev_value[tmp] = dev_acum[tmp] >> 4;
      dev_acum[tmp] = 0;      
    }
  } 

}



/*
  Digital encoders
*/
#define ENCODERS_QTY 3
unsigned char encoder_clk [ENCODERS_QTY];
unsigned char encoder_clk_prev [ENCODERS_QTY];
unsigned char encoder_data [ENCODERS_QTY];
unsigned char encoder_data_prev [ENCODERS_QTY];
boolean encoder_ignore_next [ENCODERS_QTY];

void encoder_tick() {

    encoder_data[0] = (PINB & B00010000) ? 1 : 0; 
    encoder_clk[0]  = (PINB & B00100000) ? 1 : 0;
    encoder_data[1] = (PIND & B00000010) ? 0 : 1; /* Inverting just to match with thumb rotating up */
    encoder_clk[1]  = (PINB & B10000000) ? 1 : 0;
    encoder_data[2] = (PINC & B10000000) ? 1 : 0;
    encoder_clk[2]  = (PINC & B01000000) ? 1 : 0;

    for (unsigned char cnt=0; cnt<ENCODERS_QTY; cnt++) {

      if ( encoder_clk[cnt] != encoder_clk_prev[cnt]  ) {

        if ( false == encoder_ignore_next[cnt] ) {
          /* encoder valid event (up or down) */
          encoder_ignore_next[cnt] = true;
        } else {
          encoder_ignore_next[cnt] = false;
        }
      encoder_data_prev[cnt] = encoder_data[cnt];
      encoder_clk_prev[cnt] = encoder_clk[cnt];
    }
  }           
}


/*
 * 0, 65, 128, 207, 282, 421, 511, 605, 696, 813, 896, 1023
 *  +- 30 ADC points safe window
 */
/* 12 pos mechanical switch on the board wired to adc */
const unsigned int switch_limits[] = { 45, 108, 187, 262, 401, 491, 585, 676, 793, 876, 993 };

char switch_analog2digital( unsigned int value ) {
  for (unsigned char aux=0; aux<12; aux++) {
    if ( value < switch_limits[aux] ) return (aux);
  }
  return(11);
}

/* 6 buttons wired to ADC */
const unsigned int btn_limits[] = { 435, 472, 509, 530, 543, 544 };
char btn_analog2digital( unsigned int value ) {  
  for (char aux=0; aux<6; aux++) {
    if ( value < btn_limits[aux] ) { 
      return (aux);
    }
  }
  return(-1);
}


#define QTD_DIGITAL_POS_SWITCH 12
#define QTD_DIGITAL_INPUT 3
#define QTD_ANALOG_BTN_PER_SET 6
#define BTN_DIGITAL_NUM QTD_DIGITAL_INPUT + 3*QTD_DIGITAL_POS_SWITCH + 2*QTD_ANALOG_BTN_PER_SET
#define JOY_BTN_MAX 32

boolean btn_digital [ BTN_DIGITAL_NUM ];
boolean btn_digital_prev [ BTN_DIGITAL_NUM ];

/* Joystick button map (source index in btn_digital -> joystick button id)
   Priority: discrete buttons, analog sets (PF5/PF7), then 5 positions of each rotary */
const unsigned char joystick_map[JOY_BTN_MAX] = {
  0, 1, 2,                 /* digital inputs */
  39, 40, 41, 42, 43, 44,  /* BTN_SET1 (PF5) */
  45, 46, 47, 48, 49, 50,  /* BTN_SET2 (PF7) */
  3, 4, 5, 6, 7,           /* Rotary1 first 5 */
  15, 16, 17, 18, 19,      /* Rotary2 first 5 */
  27, 28, 29, 30, 31,      /* Rotary3 first 5 */
  0, 0                     /* spare slots mapped to btn 0 */
};

void joystick_sync() {
  for (unsigned char btn_id = 0; btn_id < JOY_BTN_MAX; btn_id++) {
    unsigned char src = joystick_map[btn_id];
    boolean state = false;
    if (src < BTN_DIGITAL_NUM) {
      state = btn_digital[src];
    }
    Joystick.setButton(btn_id, state);
  }
}

/* everything is a "button" on joystick. push-buttom, rotate switch and encoders 
   btn_digital array contains boolean state of each button 
*/
void btn_tick() {

  /* Digital Inputs (encoder and shifter buttons */
  btn_digital[0] = (PINE & B01000000) ? false : true; /* Enc3 Btn */
  btn_digital[1] = (PIND & B00000001) ? true : false; /* Shift 1*/
  btn_digital[2] = (PIND & B10000000) ? true : false; /* Shift 2*/
  for (unsigned char cnt=QTD_DIGITAL_INPUT; cnt<BTN_DIGITAL_NUM; cnt++) {
    btn_digital[cnt] = false; /* switch has only 1 position enabled each time*/
  }

  /* Analog Inputs Switches. trick with button, not good to read I know but save a lot of code ;) sorry  */
  btn_digital[ QTD_DIGITAL_INPUT + switch_analog2digital(dev_value[0]) ] = true;
  btn_digital[ QTD_DIGITAL_INPUT + QTD_DIGITAL_POS_SWITCH + switch_analog2digital(dev_value[1]) ] = true;
  btn_digital[ QTD_DIGITAL_INPUT + (2*QTD_DIGITAL_POS_SWITCH) + switch_analog2digital(dev_value[2]) ] = true;

  /* Analog Input Buttons */
  char btn_set_analog = btn_analog2digital( dev_value[3] );
    if (  btn_set_analog >= 0 ) {
    btn_digital[ QTD_DIGITAL_INPUT + (3*QTD_DIGITAL_POS_SWITCH) + btn_set_analog ] = true;
  }
  
  btn_set_analog = btn_analog2digital( dev_value[4] );
  if (  btn_set_analog >= 0 ) {
    btn_digital[ QTD_DIGITAL_INPUT + (3*QTD_DIGITAL_POS_SWITCH) + QTD_ANALOG_BTN_PER_SET + btn_set_analog ] = true;
  }
  
  for (unsigned char cnt=0; cnt<BTN_DIGITAL_NUM; cnt++) {
    if ( btn_digital_prev[cnt] != btn_digital[cnt] ) {
      /* value change */
      Serial.println("");
      Serial.print("Btn");
      Serial.print(cnt);
      Serial.print(" = ");
      Serial.print( btn_digital[cnt] );
    }
    btn_digital_prev[cnt] = btn_digital[cnt];


  }
  joystick_sync();
  
}



void loop() {
  static unsigned char teste=0; //board test  aux
  static unsigned char teste_aux = 0;// board test aux

  /* PCB test */
  led_update_percent( 1 );

  if ( global_timer.flags.flag.on1s ) {
    led_update_aux( teste_aux );
    if ( teste_aux == 0 ) 
      teste_aux = 1;
    else
      teste_aux = 0;
  } 

 /* ** */
  if ( 1 == global_timer.flags.flag.on512us ) {
    analog_tick();
  }

  if ( 1 == global_timer.flags.flag.on512us ) {
    led_tick();
  }

  if ( 1 == global_timer.flags.flag.on1ms ) {
    encoder_tick();
  }

  if ( 1 == global_timer.flags.flag.on10ms ) {
    btn_tick();
  }

  global_timer.flags.all = 0;
  while ( global_timer.flags.flag.on512us == 0 ) { /* just wait dude ! */ }
}
