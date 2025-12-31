#include "globals.h"
#include "leds.h"

T_LED leds[ NUM_LEDS ];


#define RPM_LEDS_NUM 14           /* leds[6] - leds [19] */
#define RPM_FIRST_LED_INDEX 6
#define RPM_LEDS_STEP 8           /* each 8% turn on one led */
#define RPM_FIRST_RED_INDEX 8
#define RPM_FIRST_PURPLE_INDEX 16

void led_update_percent( unsigned char value ) {

  /*static char led_cnt = 0;
  static char color_cnt = 0;
  leds[ led_cnt ].enabled = 1;
  leds[ led_cnt ].color = color_cnt;
  if ( 1 == global_timer.flags.flag.on1s ) {
    color_cnt++;
    if (color_cnt >6) { color_cnt = 0; }
  }*/

  for (unsigned char cnt=RPM_FIRST_LED_INDEX; cnt<RPM_LEDS_NUM+RPM_FIRST_LED_INDEX;  cnt++) {
        
      leds[ cnt ].enabled = ( value >= (RPM_LEDS_STEP * (cnt-RPM_FIRST_LED_INDEX-1)) );

      if (cnt > RPM_FIRST_PURPLE_INDEX) {
        leds[ cnt ].color = PURPLE; 
      } else {
        if ( cnt > RPM_FIRST_RED_INDEX ) {
          leds[ cnt ].color = RED;        
        } else {
          leds[ cnt ].color = GREEN;        
        }
      }
  }
  
}


void led_update_aux( unsigned char value ) {

  for (unsigned char aux=0; aux<6; aux++) {
    if ( value > 0 ) {
      leds[ aux ].enabled = true;
      leds[ aux ].color = RED;
    } else {
      leds[ aux ].enabled = false;
    }
  }  
  leds[ 0 ].enabled = true;
  leds[ 3 ].enabled = true;
  leds[ 0 ].color = BLUE;
  leds[ 3 ].color = BLUE;
}



void led_tick( ) {
  /* Only ONE LED should ever be enabled at a time (hardware brightness constraint).
     For best duty-cycle, scan only the active LED instead of cycling through all outputs. */
  signed char active = -1;
  for (unsigned char i = 0; i < NUM_LEDS; i++) {
    if (leds[i].enabled) {
      if (active < 0) {
        active = (signed char)i;
      } else {
        /* enforce single-enabled */
        leds[i].enabled = false;
      }
    }
  }

  if (active < 0) {
    /* nothing active -> keep outputs off */
    led_scan_update(0);
  } else {
    led_scan_update((unsigned char)active);
  }
}



void led_scan_update( unsigned char led_num ) {

  DATA_DOWN;
  STCP_DOWN;
  SHCP_DOWN;

  PORTD = PORTD | B01110000; //OFF
  
  for ( char shifter_cnt=NUM_LEDS-1; shifter_cnt>=0; shifter_cnt--) {

      if ( (shifter_cnt == led_num) && leds[ shifter_cnt ].enabled ) { 
        DATA_UP;
      } else {
        DATA_DOWN;
      }

      switch ( leds[led_num].color ) {
        case BLUE:
            PORTD = PORTD & BLUE_MASK; 
            break;
        case RED:
            PORTD = PORTD & RED_MASK; 
            break;
        case GREEN:
            PORTD = PORTD & GREEN_MASK; 
            break;
        case YELLOW:
            PORTD = PORTD & GREEN_MASK & RED_MASK; 
            break;
        case PURPLE:
            PORTD = PORTD & BLUE_MASK & RED_MASK; 
            break;        
        case CYAN:
            PORTD = PORTD & BLUE_MASK & GREEN_MASK; 
            break;                    
        default:
          PORTD = 0B11111111;             
      }
                  
        
      SHCP_UP;
      SHCP_DOWN;
      delayMicroseconds(1);
  }
  STCP_UP;  
}
