#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#define DATA3_DIR DDRD
#define DATA3_PORT PORTD
#define DATA3_IN PIND
#define DATA3_SHIFT 0
#define DATA5_DIR DDRB
#define DATA5_PORT PORTB
#define DATA5_IN PINB
#define DATA5_SHIFT 0
//+5V = SNES, GND = NES
#define SNES_DIR DDRF
#define SNES_PORT PORTF
#define SNES_IN PINF
#define SNES_SHIFT 4
//D7 = clock, D6 = latch
#define CLOCK_DIR DDRD
#define CLOCK_PORT PORTD
#define CLOCK_IN PIND
#define CLOCK_MASK 0x80
#define LATCH_DIR DDRD
#define LATCH_PORT PORTD
#define LATCH_IN PIND
#define LATCH_MASK 0x40

//For now, just the ports I have populated...
#define IO_MASK 0x0F

//C6/7 = 9/10
#define PIN_TRIGGER 10

#endif /* PIN_CONFIG_H */
