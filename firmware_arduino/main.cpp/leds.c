#include "globals.h"
#include "leds.h"

T_LED leds[ NUM_LEDS ];

/* RGB cathode pins are on PORTD bits 4..6 (PD4=BLUE, PD5=RED, PD6=GREEN).
   We can reduce color leakage by floating (INPUT, no pullup) the non-selected channels. */
#define RGB_PORTD_MASK B01110000
#define BLUE_BIT_MASK  B00010000 /* PD4 */
#define RED_BIT_MASK   B00100000 /* PD5 */
#define GREEN_BIT_MASK B01000000 /* PD6 */

static inline void rgb_float_all() {
  /* INPUT, no pullups on PD4..PD6 */
  DDRD  = DDRD  & ~RGB_PORTD_MASK;
  PORTD = PORTD & ~RGB_PORTD_MASK;
}

static inline void rgb_enable_channel(unsigned char channel) {
  /* Default: float all */
  rgb_float_all();

  switch (channel) {
    case RED:
      DDRD  = DDRD  | RED_BIT_MASK;    /* output */
      PORTD = PORTD & ~RED_BIT_MASK;   /* drive low (sink) */
      break;
    case GREEN:
      DDRD  = DDRD  | GREEN_BIT_MASK;
      PORTD = PORTD & ~GREEN_BIT_MASK;
      break;
    case BLUE:
      DDRD  = DDRD  | BLUE_BIT_MASK;
      PORTD = PORTD & ~BLUE_BIT_MASK;
      break;
    default:
      /* OFF: float all */
      break;
  }
}


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
  /* Fixed RGB multiplex with blanking gaps:
     - RED channel ON for 10ms, OFF for 5ms
     - GREEN channel ON for 9ms, OFF for 5ms
     - BLUE channel ON for 11ms, OFF for 5ms
     (1 tick == 1ms, called from loop() on on1ms flag)

     Channel phases include composite colors:
       RED phase: RED, YELLOW, PURPLE
       GREEN phase: GREEN, YELLOW, CYAN
       BLUE phase: BLUE, PURPLE, CYAN
  */
  static unsigned char prev_mask = 0;
  static unsigned char phase_len = 0;
  static unsigned char phase_idx = 0;
  static unsigned char elapsed = 0; /* ms elapsed in current phase */
  static unsigned char phase_color[6];
  static unsigned char phase_dur[6];

  /* If nothing is enabled, keep everything OFF (don't cycle RGB phases) */
  boolean any_enabled = false;
  for (unsigned char i = 0; i < WIRED_LEDS; i++) {
    if (leds[i].enabled) {
      any_enabled = true;
      break;
    }
  }
  if (!any_enabled) {
    prev_mask = 0;
    phase_len = 0;
    phase_idx = 0;
    elapsed = 0;
    led_scan_update_color(OFF);
    return;
  }

  /* Build channel mask (bit0=R, bit1=G, bit2=B) based on enabled LEDs/colors */
  unsigned char mask = 0;
  for (unsigned char i = 0; i < WIRED_LEDS; i++) {
    if (!leds[i].enabled) continue;
    unsigned char c = leds[i].color;
    if (c == RED || c == YELLOW || c == PURPLE) mask |= 0x01;
    if (c == GREEN || c == YELLOW || c == CYAN) mask |= 0x02;
    if (c == BLUE || c == PURPLE || c == CYAN) mask |= 0x04;
  }
  if (mask == 0) {
    led_scan_update_color(OFF);
    return;
  }

  /* If mask changed, rebuild the phase list and apply immediately */
  if (mask != prev_mask) {
    prev_mask = mask;
    phase_len = 0;
    phase_idx = 0;
    elapsed = 0;

    /* Single channel: keep it ON continuously (no OFF gaps) */
    if (mask == 0x01) {
      phase_color[0] = RED;   phase_dur[0] = 255;
      phase_len = 1;
    } else if (mask == 0x02) {
      phase_color[0] = GREEN; phase_dur[0] = 255;
      phase_len = 1;
    } else if (mask == 0x04) {
      phase_color[0] = BLUE;  phase_dur[0] = 255;
      phase_len = 1;
    } else {
      /* Multi-channel: apply OFF gap between active channels */
      if (mask & 0x01) { phase_color[phase_len] = RED;   phase_dur[phase_len++] = RGB_RED_ON_MS;   phase_color[phase_len] = OFF; phase_dur[phase_len++] = RGB_OFF_MS; }
      if (mask & 0x02) { phase_color[phase_len] = GREEN; phase_dur[phase_len++] = RGB_GREEN_ON_MS; phase_color[phase_len] = OFF; phase_dur[phase_len++] = RGB_OFF_MS; }
      if (mask & 0x04) { phase_color[phase_len] = BLUE;  phase_dur[phase_len++] = RGB_BLUE_ON_MS;  phase_color[phase_len] = OFF; phase_dur[phase_len++] = RGB_OFF_MS; }
    }

    led_scan_update_color(phase_color[0]);
    return;
  }

  /* Advance time in current phase */
  unsigned char dur = phase_dur[phase_idx];
  if (dur != 255) {
    if (elapsed < (unsigned char)(dur - 1)) {
      elapsed++;
      return;
    }
    elapsed = 0;
  }

  /* Next phase */
  phase_idx++;
  if (phase_idx >= phase_len) phase_idx = 0;
  led_scan_update_color(phase_color[phase_idx]);

}

void led_scan_update_color( unsigned char color ) {

  DATA_DOWN;
  STCP_DOWN;
  SHCP_DOWN;

  /* IMPORTANT: avoid color "leak" while shifting/latching.
     Keep RGB lines OFF during the shift, latch STCP, then enable the selected color. */
  rgb_float_all();

  for ( char shifter_cnt=NUM_LEDS-1; shifter_cnt>=0; shifter_cnt--) {

      boolean on = false;
      if (shifter_cnt < WIRED_LEDS) {
        /* RGB channel selection: include composite colors */
        if (leds[(unsigned char)shifter_cnt].enabled && (color != OFF)) {
          unsigned char c = leds[(unsigned char)shifter_cnt].color;
          if (color == RED) {
            on = (c == RED) || (c == YELLOW) || (c == PURPLE);
          } else if (color == GREEN) {
            on = (c == GREEN) || (c == YELLOW) || (c == CYAN);
          } else if (color == BLUE) {
            on = (c == BLUE) || (c == PURPLE) || (c == CYAN);
          } else {
            on = (c == color);
          }
        }
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

  /* Now enable the selected color after the new pattern is latched */
  rgb_enable_channel(color);
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
