/* 
 * File:   leds.h
 * Author: robertoalcantara
*/

#ifndef LEDS_H
  #define  LEDS_H
  #include "Arduino.h"
  
  
  /*
   *  DS - Data input
   *  SHCP - SRCLK  - shift register clock 
   *  STCP - RCLK - storage register clock
   */
  #define NUM_LEDS 24 //20 wired, 24 shift outputs
  #define WIRED_LEDS 24

  /* RGB multiplex timings (ms) */
  #define RGB_RED_ON_MS   10
  #define RGB_GREEN_ON_MS 9
  #define RGB_BLUE_ON_MS  11
  #define RGB_OFF_MS      5

  #define OFF 0
  #define RED 1
  #define GREEN 2
  #define BLUE 3
  #define YELLOW 4
  #define PURPLE 5
  #define CYAN 6
  
  #define DS_PORTB_MASK   B00000010 
  #define SHCP_PORTB_MASK B00001000 
  #define STCP_PORTB_MASK B00000100 
  
  #define SHCP_UP PORTB = PORTB | SHCP_PORTB_MASK 
  #define SHCP_DOWN PORTB = PORTB & ~SHCP_PORTB_MASK 
  
  #define STCP_UP PORTB = PORTB | STCP_PORTB_MASK 
  #define STCP_DOWN PORTB = PORTB & ~STCP_PORTB_MASK 
  
  #define DATA_UP PORTB = PORTB | DS_PORTB_MASK 
  #define DATA_DOWN PORTB = PORTB & ~DS_PORTB_MASK 
  
  #define BLUE_MASK  0B11101111
  #define RED_MASK   0B11011111
  #define GREEN_MASK 0B10111111
  
  typedef struct t_led {
    unsigned char enabled;
    unsigned char color;
  } T_LED;
  
  
  extern T_LED leds[NUM_LEDS];
  
  void led_scan_update( unsigned char led_num );
  void led_scan_update_color( unsigned char color );
  void led_tick( );
  
#endif
