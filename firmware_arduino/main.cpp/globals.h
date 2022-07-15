/* 
 * File:   globals.h
 * Author: robertoalcantara
*/

#ifndef GLOBALS_H
  #define  GLOBALS_H

  #define DEBUG 1
  
  union flag_def {
     struct {
      unsigned on512us :1;
      unsigned on1ms :1;
      unsigned on10ms :1;
      unsigned on100ms :1;
      unsigned on1s  :1;
      unsigned _extra :4;
    } flag;
    unsigned char all;
  };
  
  typedef struct t_global_timer {
    union flag_def  flags;
    unsigned char aux_1ms;
    unsigned char aux_10ms;
    unsigned char aux_100ms;
    unsigned char aux_1s;
  } T_GLOBAL_TIMER ;
  
  extern volatile T_GLOBAL_TIMER global_timer;

#endif
