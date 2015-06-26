// lcd-routine.h
// Ansteuerung eines HD44780 kompatiblen LCD im 4-Bit-Interfacemodus
// http://www.mikrocontroller.net/articles/AVR-GCC-Tutorial

void lcd_data(unsigned char temp1);
void lcd_string(char *data);

void lcd_enable(void);
void lcd_init(void);
void lcd_home(void);
void lcd_clear(void);
void set_cursor(uint8_t x, uint8_t y);

// Hier die verwendete Taktfrequenz in Hz eintragen, wichtig!
 
//#define F_CPU 16000000
 
// LCD Befehle
 
#define CLEAR_DISPLAY 0x01
#define CURSOR_HOME   0x02
 
// Pinbelegung f¸r das LCD, an verwendete Pins anpassen
 
#define LCD_PORT_4    PORTC
#define LCD_DDR_4     DDRC
#define LCD_D4      PC0

#define LCD_PORT_5    PORTC
#define LCD_DDR_5     DDRC
#define LCD_D5      PC1

#define LCD_PORT_6    PORTC
#define LCD_DDR_6     DDRC
#define LCD_D6      PC2

#define LCD_PORT_7    PORTC
#define LCD_DDR_7     DDRC
#define LCD_D7      PC3

#define LCD_RS_PORT    PORTC
#define LCD_RS_DDR    DDRC
#define LCD_RS        PC4

#define LCD_EN1_PORT  PORTC
#define LCD_EN1_DDR    DDRC
#define LCD_EN1       PC5

