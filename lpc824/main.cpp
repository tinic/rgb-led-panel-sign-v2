#include "LPC82x.h"

#define LED0		2
#define LED1		3

#define STB 		14	// latch
#define CLK			15	// clock
#define OE			16	// led on/off

#define COL_R0		17
#define COL_G0		18
#define COL_B0		19
#define COL_R1		20
#define COL_G1		21
#define COL_B1		22

#define ADDR_A		24
#define ADDR_B		25
#define ADDR_C		26
#define ADDR_D		27

static uint8_t *display_mem = (uint8_t *)(0x10000400);

static void output_line(const uint16_t *line, uint32_t pl, uint32_t pg) {
	LPC_GPIO_PORT->MASK0 = ~((1<<CLK)|
			   	   	   	     (1<<OE)|
			                 (1<<COL_R0)|
							 (1<<COL_G0)|
							 (1<<COL_B0)|
							 (1<<COL_R1)|
							 (1<<COL_G1)|
							 (1<<COL_B1)); // setup pixel mask

	uint32_t pc = 0b10000000100000001000000010000000;
	uint32_t pa = 0b00000001000000010000000100000001;
	uint32_t ma = 0b01000000010000000100000001000000;

	volatile uint32_t *clr0 = &LPC_GPIO_PORT->CLR0;
	volatile uint32_t *mpin0 = &LPC_GPIO_PORT->MPIN0;
	volatile uint32_t *set0 = &LPC_GPIO_PORT->SET0;

	for (uint32_t c = 0; c < pl; c++) {
		*clr0 = (1<<STB); // clear line latch
		const uint16_t *src = line;

		uint32_t x = 2;
		*set0 = ((( pg - x ) >> 31 ) << OE);  // gamma 1?
#if 1
// This code is optimized based on a
// specific pin configuration
		uint32_t a; 
		uint32_t b;
		uint32_t s;

#define DO_2PIXELS \
		a = (pc - ((src[1] << 16) | src[0])) & ma; \
		b = (pc -   src[2]                 ) & ma; \
		s = \
		   ((( pg - x++ ) >> 31 ) << OE)| \
		   (( a << 11 )| \
			( a <<  4 )| \
			( a >>  3 )| \
			( a >> 10 )| \
			( b << 15 )| \
 		    ( b <<  8 )); \
		*mpin0 = s; \
 		s |= (1<<CLK); \
		*mpin0 = s; \
		src += 3;

		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;
		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;
		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;
		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;

		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;
		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;
		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;
		DO_2PIXELS; DO_2PIXELS; DO_2PIXELS; DO_2PIXELS;

#else  // #if 1
		do {
			uint32_t a = (pc - ((src[1] << 16) | src[0])) & ma;
			uint32_t b = (pc -   src[2]                 ) & ma;
		 	uint32_t set =
	 		    ((( pg - x++ ) >> 31 ) << OE)|
  #if 1
	 			// This code is optimized based on a
		 		// specific pin configuration
				(( a << 11 )|
				 ( a <<  4 )|
				 ( a >>  3 )|
				 ( a >> 10 )|
				 ( b << 15 )|
				 ( b <<  8 ));
  #else  // #if 1
				( ( ( a >>  6 ) & 1 ) << COL_R0 )| // 17
				( ( ( a >> 14 ) & 1 ) << COL_G0 )| // 18
				( ( ( a >> 22 ) & 1 ) << COL_B0 )| // 19
				( ( ( a >> 30 ) & 1 ) << COL_R1 )| // 20
				( ( ( b >>  6 ) & 1 ) << COL_G1 )| // 21
				( ( ( b >> 14 ) & 1 ) << COL_B1 ); // 22
  #endif  // #if 1
		 	*mpin0 = set;
		 	set |= (1<<CLK); // latch led and check gamma >  2
		 	*mpin0 = set;
		 	src += 3;
		} while ( x <= 33 );

#endif  // #if 0
		*set0 = (1<<STB); // latch line
		*clr0 = (1<<OE); // led on
		*set0 = ((( pg - 1 ) >> 31 ) << OE); // gamma 0?
		pc += pa;
	}
}

static void output_frame() {
	static uint8_t interlace_pattern[16] = {
			15, 13, 11, 9, 7, 5, 3, 1,
			14, 12, 10, 8, 6, 4, 2, 0
	};
	static uint32_t led_blink_count = 0;
	if ((led_blink_count++ & 0x08)) {
		LPC_GPIO_PORT->SET0 = (1<<LED1);
	} else {
		LPC_GPIO_PORT->CLR0 = (1<<LED1);
	}
	const uint16_t *lines = (uint16_t *)display_mem;
	volatile uint32_t pl = 32;
	volatile uint32_t pg = 2;
	for (uint32_t y = 0; y < 16; y++) {
		LPC_GPIO_PORT->SET0 = (1<<OE); // led off
		LPC_GPIO_PORT->MASK0 = ~((1<<ADDR_A)|
								 (1<<ADDR_B)|
								 (1<<ADDR_C)|
								 (1<<ADDR_D)); // setup address mask
		LPC_GPIO_PORT->MPIN0 = interlace_pattern[y] << ADDR_A;
		output_line(lines, pl, pg);
		lines += 32*3;
	}
}

static uint32_t get_offset(uint32_t x, uint32_t y) {
	static uint8_t interlace_pattern[16] = {
			15,  7, 14,  6, 13,  5, 12,  4,
			11,  3, 10,  2,  9,  1,  8,  0
	};
	uint32_t rx = x;
	uint32_t ry = y & 0x0F;
	uint32_t ty = y & 0x10;
	return ((interlace_pattern[ry]) * 32 * 6) + (ty ? 3 : 0) + ( (rx & 0x1F) * 6 );
}

static void gradient_test() {
	static uint32_t p = 0;
	for (uint32_t y = 0; y < 32; y++) {
		for (uint32_t x = 0; x < 32; x++) {
			uint32_t offset = get_offset(x+p,y+(p>>4));
			display_mem[offset+2] = (x * 32) / 32;
			display_mem[offset+1] = (y * 32) / 32;
			display_mem[offset+0] = ((63 - x - y)/2 * 32) / 32;
		}
	}
	p++;
}


int main(void) {

    /* Pin Assign 8 bit Configuration */
    /* none */
    /* Pin Assign 1 bit Configuration */
    /* RESET */
    LPC_SWM->PINENABLE0 = 0xfffffeffUL;

    // set pins to output
	LPC_GPIO_PORT->DIR0 |= (1 << LED0);
	LPC_GPIO_PORT->DIR0 |= (1 << LED1);

    LPC_GPIO_PORT->DIR0 |= (1 << OE);
	LPC_GPIO_PORT->DIR0 |= (1 << CLK);
	LPC_GPIO_PORT->DIR0 |= (1 << STB);

	LPC_GPIO_PORT->DIR0 |= (1 << COL_R0);
	LPC_GPIO_PORT->DIR0 |= (1 << COL_G0);
	LPC_GPIO_PORT->DIR0 |= (1 << COL_B0);
	LPC_GPIO_PORT->DIR0 |= (1 << COL_R1);
	LPC_GPIO_PORT->DIR0 |= (1 << COL_G1);
	LPC_GPIO_PORT->DIR0 |= (1 << COL_B1);

	LPC_GPIO_PORT->DIR0 |= (1 << ADDR_A);
	LPC_GPIO_PORT->DIR0 |= (1 << ADDR_B);
	LPC_GPIO_PORT->DIR0 |= (1 << ADDR_C);
	LPC_GPIO_PORT->DIR0 |= (1 << ADDR_D);

	LPC_GPIO_PORT->SET0 = (1<<LED0);

	uint32_t gamma = 1;
	gradient_test();
	while(1) {
		gradient_test();
    	output_frame();
    	gamma += 1;
    }

    return 0;
}

