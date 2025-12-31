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
  /* Multiplex scan by COLOR:
     - In one scan slot we can light MULTIPLE LEDs at once, as long as they share the same color
       (because color lines are global).
     - If multiple colors are enabled, we time-multiplex between colors.
  */
  static unsigned char dwell_ms = 0;
  static unsigned char current_color = OFF; /* OFF means "nothing latched" */

  /* Determine which colors are currently used */
  boolean color_used[CYAN + 1];
  for (unsigned char c = 0; c <= CYAN; c++) color_used[c] = false;

  unsigned char any_enabled = 0;
  for (unsigned char i = 0; i < WIRED_LEDS; i++) {
    if (leds[i].enabled && leds[i].color >= RED && leds[i].color <= CYAN) {
      color_used[leds[i].color] = true;
      any_enabled = 1;
    }
  }

  if (!any_enabled) {
    current_color = OFF;
    dwell_ms = 0;
    led_scan_update_color(OFF);
    return;
  }

  /* If the current color is still used, keep it latched for LED_DWELL_MS */
  if (current_color != OFF && color_used[current_color]) {
    if (dwell_ms < LED_DWELL_MS) {
      dwell_ms++;
      return;
    }
  }
  dwell_ms = 0;

  /* Pick next used color (round-robin) */
  unsigned char next_color = OFF;
  for (unsigned char step = 0; step < CYAN; step++) {
    current_color++;
    if (current_color > CYAN) current_color = RED;
    if (color_used[current_color]) {
      next_color = current_color;
      break;
    }
  }
  if (next_color == OFF) {
    current_color = OFF;
    led_scan_update_color(OFF);
  } else {
    current_color = next_color;
    led_scan_update_color(current_color);
  }
}

void led_scan_update_color( unsigned char color ) {

  DATA_DOWN;
  STCP_DOWN;
  SHCP_DOWN;

  /* default: all colors off */
  PORTD = PORTD | B01110000; //OFF

  switch ( color ) {
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
      /* OFF */
      PORTD = PORTD | B01110000;
  }

  for ( char shifter_cnt=NUM_LEDS-1; shifter_cnt>=0; shifter_cnt--) {

      boolean on = false;
      if (shifter_cnt < WIRED_LEDS) {
        on = (leds[(unsigned char)shifter_cnt].enabled) && (leds[(unsigned char)shifter_cnt].color == color) && (color != OFF);
      }

      if ( on ) {
        DATA_UP;
      } else {
        DATA_DOWN;
      }

      SHCP_UP;
      SHCP_DOWN;
      delayMicroseconds(1);
  }
  STCP_UP;
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
