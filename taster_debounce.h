
#ifndef F_CPU
#define F_CPU           16000000                   // processor clock frequency
#warning kein F_CPU definiert
#endif
 
#define KEY_DDR         DDRA
#define KEY_PORT        PORTA
#define KEY_PIN         PINA
#define KEY0            0
#define KEY1            1
#define KEY2            2
#define KEY3            3
#define KEY4            4
#define KEY5            5

#define ALL_KEYS        (1<<KEY0 | 1<<KEY1 | 1<<KEY2 | 1<<KEY3 | 1<<KEY4 | 1<<KEY5)
 
#define REPEAT_MASK     (1<<KEY2 | 1<<KEY3)       // repeat: key1, key2
#define REPEAT_START    50                        // after 500ms
#define REPEAT_NEXT     10                        // every 200ms
 
#define LED_DDR         DDRD
#define LED_PORT        PORTD
#define LED0            7
#define LED1            5
#define LED2            6

uint8_t get_key_press( uint8_t key_mask );
uint8_t get_key_rpt( uint8_t key_mask );
uint8_t get_key_short( uint8_t key_mask );
uint8_t get_key_long( uint8_t key_mask );

