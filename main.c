//		main.c

#include <avr/version.h>
#if __AVR_LIBC_VERSION__ < 10606UL
#error update to avrlibc 1.6.6 or newer
#endif

#ifndef EEMEM
#define EEMEM  __attribute__ ((section (".eeprom")))
#endif

#ifndef F_CPU
#define F_CPU           16000000                   // processor clock frequency
#warning kein F_CPU definiert
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>


#include "lcd-routines.h"
#include "uart.h"
#include "onewire.h"
#include "ds18x20.h"
#include "taster_debounce.c"
#define ds1820_termostat	DS18x20_termostat_a 

#define BAUD 19200

#define MAXSENSORS 1
//--------------------------------------------------------------
// Rast Temperaturen u. Zeit      50,8�C====>>5080
//--------------------------------------------------------------
unsigned int rast_w[1][13][3];



//-------------------------------------------------------------------
//------	Taster-Definition								  -------
//-------------------------------------------------------------------


char namen [12][10]={"EINMAISH"," 1.RAST "," 2.RAST "," 3.RAST "," 4.RAST "," 5.RAST "," 6.RAST "," 7.RAST ","ABMAISCH"," -PAUSE- "," KOCHEN"," -PAUSE- "};

 
#define KEY_DDR         DDRB
#define KEY_PORT        PORTB
#define KEY_PIN         PINB
#define KEY0            0
#define KEY1            1
#define KEY2            2
#define KEY3            3
#define KEY4            5
#define KEY5            4
#define KEY6			6
#define ALL_KEYS        (1<<KEY0 | 1<<KEY1 | 1<<KEY2 | 1<<KEY3 | 1<<KEY4 | 1<<KEY5 | 1<<KEY6)
 
#define REPEAT_MASK     (1<<KEY0 | 1<<KEY1 | 1<<KEY2 | 1<<KEY3 | 1<<KEY4 | 1<<KEY5 | 1<<KEY6)  
#define REPEAT_START    100                        // after 100ms
#define REPEAT_NEXT     5                        // every 5ms
 
//--------------------------------------------------------------------
// EEPROM Definieren	f�r die Speicherpl�tze
//--------------------------------------------------------------------


unsigned int eeFooWordArray1[3][13][3] EEMEM;




volatile uint8_t key_state;                                // debounced and inverted key state:
                                                  // bit = 1: key pressed
volatile uint8_t key_press;                                // key press detect
 
volatile uint8_t key_rpt;                                  // key long press and repeat

volatile int8_t start=1 ,z,rast_c,heat=0,r_stunde,r_minute,taster_a=0,taster_b=0,kochen;
volatile int8_t menue=1,u_menue=0,u_stunde,u_minute,lcd_reset=0;
unsigned int rast, stunde, minute, sekunde;
volatile unsigned int r_temp,u_temp=0;
unsigned int data;
unsigned int switsch(unsigned int a)
{ if(a==0) a=1;
else if (a==1) a=0;
return a;
}

void init (void) //l�d die StandertWerte
{
    rast_w[0][0][0]=3800;  // Temperatur beim Einmaischen

  	rast_w[0][1][0]=4500;	// erste Rast
  	rast_w[0][1][1]=0;
  	rast_w[0][1][2]=40;
							//zweite Rast
  	rast_w[0][2][0]=6500;	//Temperatur: 65,00�C
  	rast_w[0][2][1]=0;		//Stunden
  	rast_w[0][2][2]=30;		//Minuten
							//dritte Rast
  	rast_w[0][3][0]=7200;
  	rast_w[0][3][1]=0;
  	rast_w[0][3][2]=30;

	rast_w[0][0][0]=3800;  // Temperatur beim Einmaischen

  	rast_w[0][4][0]=0;
  	rast_w[0][4][1]=0;
  	rast_w[0][4][2]=0;

  	rast_w[0][5][0]=0;
  	rast_w[0][5][1]=0;
  	rast_w[0][5][2]=0;

  	rast_w[0][6][0]=0;
  	rast_w[0][6][1]=0;
  	rast_w[0][6][2]=0;

  	rast_w[0][7][0]=0;
 	rast_w[0][7][1]=0;
 	rast_w[0][7][2]=0;

  	rast_w[0][8][0]=7800;	//Abmaischen

  	rast_w[0][10][0]=9500;	//Kochen
 	rast_w[0][10][1]=1;
 	rast_w[0][10][2]=30;



//-------------------------------------------------------------------
//------- Ab-/Einschalttemperatur ist die differenz zu r_temp -------
//-------	 				50,80�C====>>5080				  -------	
//-------------------------------------------------------------------
  	rast_w[0][11][0]=100;	// Temperaturabstand �ber r_temp 
  	rast_w[0][12][0]=60;//	''		''		 unter	r_temp
}

ISR( TIMER2_OVF_vect )                            // every 10ms
{
  static uint8_t ct0, ct1, rpt;
  uint8_t i;
 
  TCNT2 = (uint8_t)(int16_t)-(F_CPU / 1024 * 10e-3 + 0.5);  // preload for 10ms
 
  i = key_state ^ ~KEY_PIN;                       // key changed ?
  ct0 = ~( ct0 & i );                             // reset or count ct0
  ct1 = ct0 ^ (ct1 & i);                          // reset or count ct1
  i &= ct0 & ct1;                                 // count until roll over ?
  key_state ^= i;                                 // then toggle debounced state
  key_press |= key_state & i;                     // 0->1: key press detect
 
  if( (key_state & REPEAT_MASK) == 0 )            // check repeat function
     rpt = REPEAT_START;                          // start delay
  if( --rpt == 0 ){
    rpt = REPEAT_NEXT;                            // repeat delay
    key_rpt |= key_state & REPEAT_MASK;
  }
}
 



///////////////////////////////////////////////////////////////////
//
// check if a key has been pressed. Each pressed key is reported
// only once
//
uint8_t get_key_press( uint8_t key_mask )
{
  cli();                                          // read and clear atomic !
  key_mask &= key_press;                          // read key(s)
  key_press ^= key_mask;                          // clear key(s)
  sei();
  return key_mask;
}
 
///////////////////////////////////////////////////////////////////
//
// check if a key has been pressed long enough such that the
// key repeat functionality kicks in. After a small setup delay
// the key is reported beeing pressed in subsequent calls
// to this function. This simulates the user repeatedly
// pressing and releasing the key.
//
uint8_t get_key_rpt( uint8_t key_mask )
{
  cli();                                          // read and clear atomic !
  key_mask &= key_rpt;                            // read key(s)
  key_rpt ^= key_mask;                            // clear key(s)
  sei();
  return key_mask;
}
 
///////////////////////////////////////////////////////////////////
//
uint8_t get_key_short( uint8_t key_mask )
{
  cli();                                          // read key state and key press atomic !
  return get_key_press( ~key_state & key_mask );
}
 
///////////////////////////////////////////////////////////////////
//
uint8_t get_key_long( uint8_t key_mask )
{
  return get_key_press( get_key_rpt( key_mask ));
}
 

ISR(TIMER0_COMP_vect){	//z�hlt die Zeit runter
z++;
if(z==125) 
	{sekunde--;
	 z=0;

	if(sekunde==-1)
		{
		sekunde=59;
		minute--;
	
		}
	if(minute==-1)
		{
		minute=59;
		stunde--;
			}
	if(stunde==-1)
		{
		if (rast>0 && rast<8)
			{
			rast++;
			heat=2;
			start=0;
			}
		stunde=9;
	
	 	}
	 }
}


 

void TIMER0_interrupt_init(void) //konfiguriert die Uhr
{
z=0; //ISR-Z�hler = 0
TCNT0=0; //Anfangsz�hlerstand = 0
OCR0=124; //Z�hler z�hlt bis 124: 15625Hz/124 =126
TCCR0=0x0d; //CTC-Modus: Takt intern von 16 Mhz /1024 =15625Hz
//Timer/Counter0 Compare Match Interrupt aktivieren:
TIMSK|=(1<<OCIE0);
sei();
}

void uhrzeit_anzeigen(void) //zeigt die Zeit an 
{ 
char buffer [20];
set_cursor(8,1);
itoa(stunde,buffer,10);
if(stunde<=9)
	{lcd_data(' ');}
	lcd_string(buffer);
	set_cursor(10,1);
	lcd_data(':');
	itoa(minute,buffer,10);
if (minute<=9){lcd_data(48);}
lcd_string(buffer);
set_cursor(13,1);

lcd_data(':');
if (sekunde<=9)
{lcd_data(48);}
itoa(sekunde,buffer,10);
lcd_string(buffer);
}

/*
int restart(void)// ein Versuch dass bei einem Neustart an der 
{				 // alten stelle weiter gebraut werden kann
stunde=0;
minute=2;
sekunde=0;
char array[2][5]={" ja  ","nein "};
int gaga=0;
while(start==1)
	{
	set_cursor(0,1);
	lcd_string("Weiter Brauen ?");
	set_cursor(0,2);
	lcd_string(array[gaga]);
	set_cursor(5,2);
	uhrzeit_anzeigen();
	if (get_key_press((1<<KEY0)||(1<<KEY1)))
		{
		gaga=switsch(gaga);
		}
	if (get_key_press(1<<KEY4))
		{
		start=0;
		}
	}
if (gaga==0)
		{
		rast=eeprom_read_byte(&eeFooRast);
		stunde=eeprom_read_byte(&eeFooStunde);
		minute=eeprom_read_byte(&eeFooMinute);
		}
return gaga;
			
}
*/

//Damit k�nnen Eingaben nochmal Best�tigt werden lassen
unsigned int bestaetigt(char text_a[8],char text_b[12],char text_c[2])
{
 int a=0;
int b=0;
char answ[2][5]={"nein"," ja "};
lcd_clear();
set_cursor(0,1);
lcd_string(text_a);
lcd_string(text_b);
lcd_string(text_c);
while(b==0)
{
set_cursor(5,2);
lcd_string(answ[a]);
_delay_ms(300);
if (get_key_press( 1<<KEY0)) a=switsch(a);
if (get_key_press( 1<<KEY1)) a=switsch(a);
if (get_key_long( 1<<KEY4)) b=1;
//if (get_key_long( 1<<KEY5)) a=0;  b=1;

}
return a;
}

 
//Auswahl des Speicherplatzes
unsigned int speicher_menu(char text[16],int platz) // platz==1 ist ohne "STANDART" / platz==0 ist mit "STANDART"
{
	char speicherplatz[5][12]={" "," SPEICHER-A"," SPEICHER-B"," SPEICHER-C","  STANDART "};
	char speichern[2][12]={"OFNE","SPEI"};
	unsigned int sp_platz=1, sp_menu=0;
	lcd_clear();
	set_cursor(0,1);
	lcd_string(text);

	while (sp_menu==0)
		{
			//lcd_clear();
			set_cursor(0,2);
			lcd_string(speicherplatz[sp_platz]);
			if (get_key_press( 1<<KEY0))
				{
					 sp_platz--;
					if (sp_platz==0) sp_platz=4-platz;
					_delay_ms(200);
				}
			 else if (get_key_press( 1<<KEY1))
				{ 
					sp_platz++;
					if (sp_platz==5-platz) sp_platz=1; 
					_delay_ms(200);
				}
			else if (get_key_long( 1<<KEY4)) sp_menu=bestaetigt(speichern[platz],speicherplatz[sp_platz],"?");
		}
return sp_platz;
}
//Ausgabe der Temperatur auf dem LCD
void temp_ausgabe(unsigned int temp)
	{
	
	char buffer_a[10];
	char buffer_b[10];
	char *ptr=buffer_a;
	char *str=buffer_b;
	itoa(temp,buffer_a,10);
	if (temp==0)
		{ 
		*str=' ';
		str++;
		*str=' ';
		str++;
		*str='0';
		str++;
		*str=',';
		str++;
		*str='0';
		str++;
		*str='0';
		str++;
		*str=223;
		str++;
		*str='C';
		str++;
		*str=' ';
		
		}

	else if (temp<10 && temp>0)
		{
		*str=' ';
		str++;
		*str=' ';
		str=str+1;
		*str='0';
		str++;
		*str=',';
		str++;
		*str='0';
		str++;
		*str=*ptr;
		str++;
		*str=223;
		str++;
		*str='C';
		str++;
		*str=' ';
		}	

	else if (temp<100 && temp>=10)
		{
		*str=' ';
		str++;
		*str=' ';
		str=str+1;
		*str='0';
		str++;
		*str=',';
		str++;
		*str=*ptr;
		str++;
		ptr++;
		*str=*ptr;
		str++;
		*str=223;
		str++;
		*str='C';
		str++;
		*str=' ';
		}
	else if (temp<1000 && temp>=100)
		{
		*str=' ';
		str++;
		*str=' ';
		str=str+1;
		*str=*ptr;
		str++;
		ptr++;
		*str=',';
		str++;
		*str=*ptr;
		str++;
		ptr++;
		*str=*ptr;
		str++;
		*str=223;
		str++;
		*str='C';
		str++;
		*str=' ';
		}
	else if (temp>=1000 && temp<10000)
		{
		int i=0;
		*str=' ';
		str++;
		for(i=0;i<5;i++)
			{
			if (i!=2)
				{
				*str=*ptr;
				ptr++;
				}
			else
				{
				*str=',';
				}
			str++;
			}
				*str=223;
				str++;
				*str='C';
				str++;
				*str=' ';
			}
	else if (temp>=10000)
		{
		int i=0;
		
		for(i=0;i<6;i++)
			{
			if (i!=3)
				{
				*str=*ptr;
				ptr++;
				}
			else
				{
				*str=',';
				}
			str++;
			}
				*str=223;
				str++;
				*str='C';
				str++;
				*str=' ';
			}	

	lcd_string(buffer_b);
	}
//Ausgabe der Zeit auf dem LCD im Men�
void time_ausgabe(int stunde,  int minute)
	{
	char buffer_a[5];
	char buffer_b[5];
	itoa(stunde,buffer_a,10);
	itoa(minute,buffer_b,10);
	lcd_string(buffer_a);
	lcd_data(':');
	if(minute<10) lcd_data('0');
	lcd_string(buffer_b);
	}
//Laden und Speichern (z.B aus dem eeprom)
void ubertrag(unsigned int b,unsigned int a, int lade)
{
	unsigned int x;
	int c=0;
	int d=0;
	
	if (lade ==0)
	{
	while(c<=13)
		{
		while(d<=3)
			{
			x=rast_w[b][c][d];
			rast_w[a][c][d]=x;
			d++;
			}
		d=0;
		c++;
		}
	}
	if (lade==1)
		{
			b=b-1;
			c=0;
			while(c<=13)
				{
				while(d<=3)
					{
					x=eeprom_read_word(&eeFooWordArray1[b][c][d]);
					rast_w[0][c][d]=x;
					d++;
					}
				d=0;
				c++;
				}
			c=0;
			
			
		}
	if(lade==2)
		{
		//a=a-1
		c=0;	
		while(c<=13)
			{d=0;
			while(d<=3)
				{
				x=rast_w[0][c][d];
				eeprom_write_word(&eeFooWordArray1[a][c][d],x);
				d++;
				}
			d=0;
			c++;
			}
		c=0;
		}

	
}
//-----Die Men�f�hrung------
//hier werden vor dem Brauen die Werte f�r die Rasts eingestellt 
void start_menu(void)
{	
	int speichern;
	int speicher;
	int menu=1;
	int h_menu=0;
	int u_menu=0;
	unsigned int offnen;
	char h_menu_name [14][15]={"EINMAISCHEN","  1.RAST   ","  2.RAST   ","  3.RAST   ","  4.RAST   ","  5.RAST   ","  6.RAST   ","  7.RAST   ","ABMAISCHEN ","PAUSE","   KOCHEN  ","TEMP-ABSCHALT","TEMP-EINSCHALT"};
	char u_menu_name [2][10]={"  TEMP:","  TIME: "};
	int unter_menu [13]={ 0,1,1,1,1,1,1,1,0,2,1,0,0};
	offnen=speicher_menu("OEFFNEN VON ?",0);
	if (offnen!=4)
	{
	ubertrag(offnen,0,1);
	}
	menu=0; 
	lcd_clear();
	while(menu==0)
		{
			set_cursor(0,1);
			lcd_string(h_menu_name[h_menu]);
			set_cursor(0,2);
			lcd_string(u_menu_name[u_menu]);
			if (u_menu==0)
				{
				temp_ausgabe(rast_w[0][h_menu][0]);
				}
			else
				{
				time_ausgabe(rast_w[0][h_menu][1],rast_w[0][h_menu][2]);
				}
			if( get_key_press( 1<<KEY2 ) || get_key_rpt( 1<<KEY2 ))
				{ 
				if (u_menu==0)
					{
					if(h_menu>=11)
						{
						rast_w[0][h_menu][0]=rast_w[0][h_menu][0]+5;
						}
					else
						{
						rast_w[0][h_menu][0]=rast_w[0][h_menu][0]+25;
						}
					}
				else
					{
					rast_w[0][h_menu][2]=rast_w[0][h_menu][2]+1;
					if(rast_w[0][h_menu][2]==60)
						{
						rast_w[0][h_menu][2]=0;
						rast_w[0][h_menu][1]=rast_w[0][h_menu][1]+1;
						}
					}
				lcd_clear();						
				}
			if( get_key_press( 1<<KEY3 ) || get_key_rpt( 1<<KEY3 ))
				{ 
				if (u_menu==0)
					{
					if(h_menu>=11)
						{
						if(rast_w[0][h_menu][0]>=5)
							{
							rast_w[0][h_menu][0]=rast_w[0][h_menu][0]-5;
							}
						else
							{
							rast_w[0][h_menu][0]=0;
							}
						}
					else
						{
						if(rast_w[0][h_menu][0]>=5)
							{
							rast_w[0][h_menu][0]=rast_w[0][h_menu][0]-25;
							}
						else
							{
							rast_w[0][h_menu][0]=0;
							}
						}
					if(rast_w[0][h_menu][0]<0)
						{
						rast_w[0][h_menu][0]=0;
						}
					}
				else
					{
					
					if(rast_w[0][h_menu][2]==0)
						{
						if (rast_w[0][h_menu][1]>0)
							{
							rast_w[0][h_menu][2]=59;
							rast_w[0][h_menu][1]=rast_w[0][h_menu][1]-1;
							
							}
						}
					else
						{
						rast_w[0][h_menu][2]=rast_w[0][h_menu][2]-1;
						}
					lcd_clear();
					}						
				}

			if( get_key_long( 1<<KEY0 ))
				{
				if( unter_menu [h_menu]==1)
					{
					u_menu=switsch(u_menu);
					lcd_clear();
					}
				}
			if( get_key_long( 1<<KEY1 ))
				{
				if( unter_menu [h_menu]==1) 
					{
					u_menu=switsch(u_menu);
					lcd_clear();
					}
				}
			if( get_key_press( 1<<KEY6 ))
				{
				if( unter_menu [h_menu]==1) 
					{
					u_menu=switsch(u_menu);
					lcd_clear();
					}
				}
			if( get_key_short( 1<<KEY1))
    			{
				h_menu++;
				while(unter_menu[h_menu]==2)
					{
					h_menu++;
					}
				if (h_menu>=13)
					{
					h_menu=0;
					}
				if (u_menu!=unter_menu[h_menu])
					{
					u_menu=0;
					}
				lcd_clear();
				}
			if( get_key_short( 1<<KEY0))
    			{
				h_menu--;
				while (unter_menu[h_menu]==2)
					{
					h_menu--;
					}
				if (h_menu<=-1)
					{
					h_menu=12;
					}
				if (u_menu!=unter_menu[h_menu])
					{
					u_menu=0;
					}
				lcd_clear();
				}
			if(get_key_long( 1<<KEY5))
				{
				menu=bestaetigt("Eingabe"," fertig ","?");
				lcd_clear();
				}
			if(get_key_long( 1<<KEY4))
				{
				menu=bestaetigt("Eingabe"," fertig ","?");
				lcd_clear();
				}
		}

speichern=bestaetigt("Werte ","speichern ","?");
if (speichern==1)	
	{
	speicher=speicher_menu("Speichern in ?",1);
	speicher--;
	ubertrag(0,speicher,2);
	}

ubertrag(0,3,2);


}


// Erkennt bei Wievielenrasts die Temperatur 0 ist
// um diese sp�ter beim Brauen zu �berspringen,
//z.b beim dr�cken auf vor und zur�ck
void taster_r(void)
{
	rast_c=1;
	if(rast_w[0][2][0]==0)
		{rast_c++;}
	if(rast_w[0][3][0]==0)
		{rast_c++;}
	if(rast_w[0][4][0]==0)
		{rast_c++;}
	if(rast_w[0][5][0]==0)
		{rast_c++;}
	if(rast_w[0][6][0]==0)
		{rast_c++;}
	if(rast_w[0][7][0]==0)
		{rast_c++;}
}

// giebt beim Brauen die gew�nschte Temperatur
//im LCD aus, als gro� "T"
void soll_temper(void)
{
int soll_temp;
soll_temp=r_temp/100;
char Buffer[20];
set_cursor(10,2);
lcd_string("T:");
itoa(soll_temp,Buffer,10);
lcd_string(Buffer);
lcd_data(223);
lcd_string("C");
}
//giebt im LCD den Namen des Brauvorgangs z.B. EINMAISCHEN aus
//und giebt die richtigen vorgegebenen Werte (Temperatur und Zeit) an eine
//Globale variable weiter, die dann von den Anderen funktionen genutzt werden.
void temperatur_rast()
{
if(rast==0)
	{
	set_cursor(0,1);
	lcd_string("EINMAISH");
	r_temp=rast_w[0][0][0]; 
	
	}
else if(rast==1)
	{
	set_cursor(0,1);
	lcd_string(" 1.RAST "); 
	r_temp=rast_w[0][1][0];
	r_stunde=rast_w[0][1][1];
	r_minute=rast_w[0][1][2];
	
	}
else if(rast==2)
	{
	if(rast_w[0][2][0]>0)
		{
		set_cursor(0,1);
		lcd_string(" 2.RAST ");
		r_temp=rast_w[0][2][0];
		r_stunde=rast_w[0][2][1];
		r_minute=rast_w[0][2][2];
		
		}
	if(rast_w[0][2][0]==0)
		{
		rast++;
		}
	}
else if(rast==3)
	{
	if(rast_w[0][3][0]>0)
		{
		set_cursor(0,1);
		lcd_string(" 3.RAST ");
		r_temp=rast_w[0][3][0];
		r_stunde=rast_w[0][3][1];
		r_minute=rast_w[0][3][2];
		
		}
	if(rast_w[0][3][0]==0)
		{
		rast++;
		}
	}
 if(rast==4)	
	{
	if(rast_w[0][4][0]>0)
		{
		set_cursor(0,1);
		lcd_string(" 4.RAST ");
		r_temp=rast_w[0][4][0]; 
		r_stunde=rast_w[0][4][1];
		r_minute=rast_w[0][4][2];
		}
	if(rast_w[0][4][0]==0)
		{
		rast++;
		}
	}	
 if(rast==5)	
	{
	if(rast_w[0][5][0]>0)
		{
		set_cursor(0,1);
		lcd_string(" 5.RAST ");
		r_temp=rast_w[0][5][0]; 
		r_stunde=rast_w[0][5][1];
		r_minute=rast_w[0][5][2];
		}
	if(rast_w[0][5][0]==0)
		{
		rast++;
		}
	}
 if(rast==6)	
	{
	if(rast_w[0][6][0]>0)
		{
		set_cursor(0,1);
		lcd_string(" 6.RAST ");
		r_temp=rast_w[0][6][0]; 
		r_stunde=rast_w[0][6][1];
		r_minute=rast_w[0][6][2];
		rast=rast-1;
		}
	if(rast_w[0][6][0]==0)
		{
		rast++;
		}
	}
 if(rast==7)	
	{
	if(rast_w[0][7][0]>0)
		{
		set_cursor(0,1);
		lcd_string(" 7.RAST ");
		r_temp=rast_w[0][7][0]; 
		r_stunde=rast_w[0][7][1];
		r_minute=rast_w[0][7][2];
		}
	if(rast_w[0][7][0]==0)
		{
		rast++;
		}
	}
 if(rast==8)
	{
	set_cursor(0,1);
	lcd_string("ABMAISCH");
	r_temp=rast_w[0][8][0];
	}
else if(rast==9)
	{
	set_cursor(0,1);
	lcd_string(" -PAUSE-");
	r_temp=0;
	kochen=2;
	}
else if(rast==10)
	{
	set_cursor(0,1);
	lcd_string(" KOCHEN");
	r_stunde=rast_w[0][9][1];
	r_minute=rast_w[0][9][2];
	kochen=1;
	}
else if(rast==11)
	{
	set_cursor(0,1);
	lcd_string(" -PAUSE-");
	r_temp=0;
	kochen=2;
	}
 if (rast>11)
	{
	rast--;
	}
}

void count_down(void)
{
	if(heat==1)
	{
		if(!(TEMP_PORT_A & (1<<TEMP_P_A)))
			{
			stunde=r_stunde;
			minute=r_minute;
			sekunde=0;
			heat=0;
			}
	}
	else if(heat>=2)
	{
	heat--;
	}
}
// die Temperaur durch an und ausschalten des 
//Heizkessels regeln
void termostat(void)
{
	unsigned int temp_aus;
	unsigned int temp_an;
	temp_aus=r_temp+rast_w[0][11][0];
	temp_an =r_temp-rast_w[0][12][0];
	ds1820_termostat(temp_an,temp_aus);
	
}
// um beim Brauen zwischen den Rasts zu wechseln
void taster_abfrage(void)
{

	if( get_key_long( 1<<KEY1 ))
		{
		if (rast<=10)
			{
			stunde=9;
			minute=59;
			sekunde=59;
			rast++;
			heat=2;
			taster_a=1;
			}
		}

	if( get_key_long( 1<<KEY0 ))		
	
		{
		if (rast>=1)
			{	
			stunde=9;
			minute=59;
			sekunde=59;
			heat=2;
			taster_b=1;
			if(rast==8)
				{
				rast=rast-rast_c;
				}
			else 
				{
				rast--;
				}
			}
		}
}			

//das Kochen
void kochen_main(unsigned int temp_kochen)
{
if(temp_kochen>=rast_w[0][9][0])
	{
	stunde=rast_w[0][9][1];
	minute=rast_w[0][9][2];
	sekunde=0;
	}
}

// nach vorhanden Temperatursensoren suchen
uint8_t search_sensors(void)
{
	uint8_t id[OW_ROMCODE_SIZE];
	uint8_t diff, nSensors;
	nSensors = 0;
	for( diff = OW_SEARCH_FIRST; 
		diff != OW_LAST_DEVICE && nSensors < MAXSENSORS ; )
	{
		DS18X20_find_sensor( &diff, &id[0] );
		nSensors++;
	}
	return nSensors;
}


int main( void )
{

lcd_init();
uart_init((UART_BAUD_SELECT((BAUD),F_CPU)));
KEY_DDR &= ~ALL_KEYS;                // konfigure key port for input
//KEY_PORT |= ALL_KEYS;                // and turn on pull up resistors
TCCR2 = (1<<CS22)|(1<<CS20)|(1<<CS21);			// durch 1024 teilen 
TIMSK = 1<<TOIE2;				// enable timer interrupt
TIMER0_interrupt_init();
init();
#ifndef OW_ONE_BUS
ow_set_bus(&PIND,&PORTD,&DDRD,PD6);
#endif
sei();
start_menu();
rast=0;
lcd_clear();
taster_r();
for(;;) // main loop
	{			
	if(rast==9)
		{temperatur_rast();
		TEMP_DDR_A |= (1<<TEMP_P_A);
		TEMP_PORT_A &= ~(1<<TEMP_P_A);	
		}
	else if(rast==11)
		{temperatur_rast();
		TEMP_DDR_A |= (1<<TEMP_P_A);
		TEMP_PORT_A &= ~(1<<TEMP_P_A);
	
		}
	else if(rast==10)
		{
		temperatur_rast();
		uhrzeit_anzeigen();
		TEMP_DDR_A |= (1<<TEMP_P_A);
		TEMP_PORT_A |= (1<<TEMP_P_A);
		kochen_a();
		}
	else
	{
		temperatur_rast();
		uhrzeit_anzeigen();
		termostat();
		soll_temper();
		count_down();
	 }
	if ( DS18X20_start_meas( DS18X20_POWER_PARASITE, NULL )	== DS18X20_OK) 
			{
			_delay_ms(DS18B20_TCONV_12BIT);
			}
	#ifdef DS18X20_VERBOSE
			// all devices:
			DS18X20_start_meas( DS18X20_POWER_PARASITE, NULL );
			_delay_ms(DS18B20_TCONV_12BIT);
			DS18X20_read_meas_all_verbose();
	#endif
	taster_abfrage();	
	}
}

