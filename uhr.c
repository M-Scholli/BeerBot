/* 
ATtiny2313 @ 1 MHz
 
Timer0 kennt zusätzlich den CTC Modus 
*/
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include "lcd-routines.h"
#include <stdint.h>
 
//Variablen für die Zeit
volatile unsigned int  millisekunden;
volatile unsigned int sekunde;
volatile unsigned int minute;
volatile unsigned int stunde;
 
int time(void)
{
  // Timer 0 konfigurieren
  //TCNT0 = (1<<WGM01); // CTC Modus
  TCNT0= ((1<<CS01) | (1<<CS00)); // Prescaler 8,

  // ((1000000/8)/1000) = 125
  OCR0 =250 -1; // ### Bugfix 20091221
 
  // Compare Interrupt erlauben
  TIMSK |= (1<<OCIE0);
 
  // Global Interrupts aktivieren
  sei();
 
  
}
 
/*
Der Compare Interrupt Handler 
wird aufgerufen, wenn 
TCNT0 = OCR0A = 125-1 
ist (125 Schritte), d.h. genau alle 1 ms
*/
ISR (TIMER0_COMPA_vect)
{
  millisekunden++;
  if(millisekunden == 1000)
  {
    sekunde++;
    millisekunden = 0;
    if(sekunde == 60)
    {
      minute++;
      sekunde = 0;
    }
    if(minute == 60)
    {
      stunde++;
      minute = 0;
    }
    if(stunde == 24)
    {
      stunde = 0;
    }
  }
}

int time_lcd(void)
{
char buffer[20];
set_cursor(0,1);
itoa (stunde,buffer,10);
lcd_string(buffer);
lcd_string(":");
itoa (minute,buffer,10);
lcd_string(buffer);
lcd_string(":");
itoa (sekunde,buffer,10);
lcd_string(buffer);
}
