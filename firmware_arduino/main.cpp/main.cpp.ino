/* 
 * File:   globals.h
 * Author: robertoalcantara
 *2
 * https://github.com/MHeironimus/ArduinoJoystickLibrary
 * Just for fun. Do whatever u want.
*/


#include "globals.h"
#include "leds.c"
#include <Joystick.h>
#include <string.h>
#include <stdlib.h>

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
  /* start with all LEDs off (no demo) */
  for (unsigned char i = 0; i < NUM_LEDS; i++) {
    leds[i].enabled = false;
  }
  led_scan_update_color( OFF ); // cleanup any data on latches
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

/* Buttons wired to ADC (resistor ladder).
   SET1 design target (Rpullup=10k to VCC; each button connects a resistor to GND):
     BTN1..BTN7: 1k2, 2k7, 4k7, 8k2, 12k, 20k, 47k
   Expected ADC codes (approx): 110, 217, 327, 461, 558, 682, 843
   Use wide mid-point thresholds to avoid noisy/ambiguous regions.
*/
const unsigned int btn_limits[] = { 163, 272, 394, 510, 620, 762, 930 };
char btn_analog2digital( unsigned int value ) {  
  for (char aux=0; aux<7; aux++) {
    if ( value < btn_limits[aux] ) { 
      return (aux);
    }
  }
  return(-1);
}


#define QTD_DIGITAL_POS_SWITCH 12
#define QTD_DIGITAL_INPUT 3
#define QTD_ANALOG_BTN_SET1 7
#define QTD_ANALOG_BTN_SET2 0  /* inactive for now */
#define BTN_DIGITAL_NUM QTD_DIGITAL_INPUT + 3*QTD_DIGITAL_POS_SWITCH + QTD_ANALOG_BTN_SET1 + QTD_ANALOG_BTN_SET2
#define JOY_BTN_MAX 32
#define SERIAL_BUF_LEN 32
#define DEBUG_BTN_LOG 1

/* Analog button sets filtering */
#define ANALOG_SET_DEBOUNCE_SAMPLES 3   /* 3 * 10ms = 30ms */
#define ANALOG_SET_LOCKOUT_MS 50        /* require 50ms after all-off before next press */

boolean btn_digital [ BTN_DIGITAL_NUM ];
boolean btn_digital_prev [ BTN_DIGITAL_NUM ];
boolean led_serial_override = false; /* when true, LEDs follow serial commands (SimHub) */
char serial_buf[SERIAL_BUF_LEN];
unsigned char serial_buf_pos = 0;

/* Joystick button map (source index in btn_digital -> joystick button id)
   Priority: discrete buttons, analog sets (PF5/PF7), then 5 positions of each rotary */
const unsigned char joystick_map[JOY_BTN_MAX] = {
  0, 1, 2,                 /* digital inputs */
  39, 40, 41, 42, 43, 44, 45,  /* BTN_SET1 (PF5) - 7 buttons */
  0, 0, 0, 0, 0, 0,            /* BTN_SET2 (PF7) - inactive */
  3, 4, 5, 6, 7,           /* Rotary1 first 5 */
  15, 16, 17, 18, 19,      /* Rotary2 first 5 */
  27, 28, 29, 30, 31,      /* Rotary3 first 5 */
  0                        /* spare slot mapped to btn 0 */
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

/* Serial protocol for LEDs (ASCII, SimHub friendly)
   Commands ended by '\n':
   - L,<idx>,<color>   idx:0-23, color:0=OFF,1=RED,2=GREEN,3=BLUE,4=YELLOW,5=PURPLE,6=CYAN
   - A,<color>         set ALL wired leds to color (0=OFF clears all)
   - C                 clear all leds (off)
   - D                 disable override (return to demo mode)
*/
void process_serial_line(char *line) {
  if (line[0] == 'L') {
    char *p = strchr(line, ',');
    if (!p) return;
    unsigned char idx = atoi(p + 1);
    p = strchr(p + 1, ',');
    if (!p) return;
    unsigned char color = atoi(p + 1);
    if (idx < WIRED_LEDS && color <= CYAN) {
      /* Multi-led state is allowed; scan will multiplex (only one ON at a time in hardware) */
      if (color == OFF) {
        leds[idx].enabled = false;
      } else {
        leds[idx].enabled = true;
        leds[idx].color = color;
      }
      led_serial_override = true;
    }
  } else if (line[0] == 'A') {
    /* All LEDs test */
    char *p = strchr(line, ',');
    if (!p) return;
    unsigned char color = atoi(p + 1);
    if (color > CYAN) return;
    for (unsigned char i = 0; i < WIRED_LEDS; i++) {
      if (color == OFF) {
        leds[i].enabled = false;
      } else {
        leds[i].enabled = true;
        leds[i].color = color;
      }
    }
    led_serial_override = true;
  } else if (line[0] == 'C') {
    for (unsigned char i = 0; i < NUM_LEDS; i++) {
      leds[i].enabled = false;
    }
    led_serial_override = true;
  } else if (line[0] == 'D') {
    led_serial_override = false;
    /* demo removed: use D as a safe "stop/clear" */
    for (unsigned char i = 0; i < NUM_LEDS; i++) {
      leds[i].enabled = false;
    }
  }
}

void serial_led_tick() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      serial_buf[serial_buf_pos] = '\0';
      process_serial_line(serial_buf);
      serial_buf_pos = 0;
    } else {
      if (serial_buf_pos < (SERIAL_BUF_LEN - 1)) {
        serial_buf[serial_buf_pos++] = c;
      } else {
        serial_buf_pos = 0; /* overflow, reset */
      }
    }
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
  /* BTN_SET1 (dev_value[3]) only (SET2 inactive for now)
     Requirements:
     - enforce >=50ms between detections on same set
     - only detect a new button when ALL are off (released) */
  {
    /* SET1 state */
    static char set1_stable_btn = -1;          /* current reported button (0..6) or -1 */
    static char set1_candidate_btn = -1;       /* debounce candidate */
    static unsigned char set1_candidate_cnt = 0;
    static unsigned long set1_lockout_until_ms = 0;

    const unsigned long now = millis();
    const unsigned int adc_set1 = (unsigned int)dev_value[3];
    const unsigned char set1_base_idx = (unsigned char)(QTD_DIGITAL_INPUT + (3 * QTD_DIGITAL_POS_SWITCH));

    /* Debounce the decoded button id */
    char decoded = btn_analog2digital(adc_set1);
    if (decoded == set1_candidate_btn) {
      if (set1_candidate_cnt < 255) set1_candidate_cnt++;
    } else {
      set1_candidate_btn = decoded;
      set1_candidate_cnt = 1;
    }

    char debounced = set1_stable_btn;
    if (set1_candidate_cnt >= ANALOG_SET_DEBOUNCE_SAMPLES) {
      debounced = set1_candidate_btn;
    }

    /* State machine: allow only one button until all-off, and enforce lockout after release */
    if (set1_stable_btn < 0) {
      /* currently all-off */
      if (now >= set1_lockout_until_ms) {
        if (debounced >= 0) {
          set1_stable_btn = debounced; /* latch first detected button */
        }
      } else {
        /* still in lockout -> force all-off */
        set1_stable_btn = -1;
      }
    } else {
      /* currently latched to a button; ignore other decoded values until all-off */
      if (debounced < 0) {
        set1_stable_btn = -1;
        set1_lockout_until_ms = now + ANALOG_SET_LOCKOUT_MS;
      }
    }

    /* Apply to btn_digital[] (exactly one for SET1, or none) */
    for (unsigned char b = 0; b < QTD_ANALOG_BTN_SET1; b++) {
      btn_digital[set1_base_idx + b] = false;
    }
    if (set1_stable_btn >= 0 && set1_stable_btn < QTD_ANALOG_BTN_SET1) {
      btn_digital[set1_base_idx + (unsigned char)set1_stable_btn] = true;
    }

#if DEBUG_BTN_LOG
    /* Helpful debug */
    static unsigned char dbg_div = 0;
    dbg_div++;
    if (dbg_div >= 50) { /* ~500ms (btn_tick every 10ms) */
      dbg_div = 0;
      Serial.println("");
      Serial.print("ADC_SET1=");
      Serial.print(adc_set1);
      Serial.print(" ADC_SET2=");
      Serial.print((unsigned int)dev_value[4]);
      Serial.print(" ST1=");
      Serial.print((int)set1_stable_btn);
      Serial.print(" ST2=");
      Serial.print(-1);
    }
#endif
  }
  
  for (unsigned char cnt=0; cnt<BTN_DIGITAL_NUM; cnt++) {
#if DEBUG_BTN_LOG
    if ( btn_digital_prev[cnt] != btn_digital[cnt] ) {
      /* value change */
      Serial.println("");
      Serial.print("Btn");
      Serial.print(cnt);
      Serial.print(" = ");
      Serial.print( btn_digital[cnt] );
    }
#endif
    btn_digital_prev[cnt] = btn_digital[cnt];


  }
  joystick_sync();
  
}



void loop() {
  serial_led_tick(); /* handle SimHub serial commands for LEDs */

 /* ** */
  if ( 1 == global_timer.flags.flag.on512us ) {
    analog_tick();
  }

  /* LED scan at 1kHz (dwell handled inside led_tick) */
  if ( 1 == global_timer.flags.flag.on1ms ) {
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
