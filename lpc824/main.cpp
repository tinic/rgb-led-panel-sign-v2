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

#define member_size(type, member) sizeof(((type *)0)->member)

#define PAGE_COUNT	6

struct data_page {
	uint8_t  data[1152];
	
	uint8_t  page;
	uint8_t  pwm_length;
	uint8_t  gamma;
	uint8_t  pad[9];

	uint32_t serial;
};

static data_page data_pages[PAGE_COUNT] = { 0 };

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
	volatile uint32_t pl = 32;
	volatile uint32_t pg = 2;

	static uint32_t led_blink_count = 0;
	if ((led_blink_count++ & 0x080)) {
		LPC_GPIO_PORT->SET0 = (1<<LED1);
	} else {
		LPC_GPIO_PORT->CLR0 = (1<<LED1);
	}

	uint32_t page_location[3] = { 0 };	
	uint32_t high_serial = 0;
	for (uint32_t s = 0; s < PAGE_COUNT; s++) {
		uint32_t ds = data_pages[s].serial;
		if (ds > high_serial) {
			high_serial = ds;
			page_location[0] = 0xFFFFFFFF;
			page_location[1] = 0xFFFFFFFF;
			page_location[2] = 0xFFFFFFFF;
			for (uint32_t t = 0; t < PAGE_COUNT; t++) {
				if (data_pages[t].serial == ds) {	
					if (data_pages[t].page < 3) {
						page_location[data_pages[t].page] = t;
					}
				}
			}
			for (uint32_t t = 0; t < 3; t++) {
				if (page_location[t] == 0xFFFFFFFFUL) {
					high_serial = 0;
				}
			}
		}
	}
	
	if (high_serial == 0) {
		return;
	}

	uint32_t y = 0;
	static uint8_t screen_split[] = { 6, 6, 4 };
	for (uint32_t s = 0; s < 3; s++) {
		uint32_t ps = page_location[s];
		uint32_t pl = data_pages[ps].pwm_length;
		uint32_t pg = data_pages[ps].gamma;
		const uint16_t *lines = (uint16_t *)data_pages[ps].data;
		uint32_t sp = screen_split[s];
		for (uint32_t p = 0; p < sp; p++) {
			static uint8_t interlace_pattern[16] = {
					15, 13, 11, 9, 7, 5, 3, 1,
					14, 12, 10, 8, 6, 4, 2, 0
			};
			LPC_GPIO_PORT->SET0 = (1<<OE); // led off
			LPC_GPIO_PORT->MASK0 = ~((1<<ADDR_A)|
									 (1<<ADDR_B)|
									 (1<<ADDR_C)|
									 (1<<ADDR_D)); // setup address mask
			LPC_GPIO_PORT->MPIN0 = interlace_pattern[y] << ADDR_A;
			output_line(lines, pl, pg);
			lines += 32*3;
			y++;
		}
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
	static uint32_t s = 0;	
	static uint32_t d = 0;

	uint32_t page_size =member_size(data_page,data);

	data_pages[d+0].serial = s;
	data_pages[d+0].page = 0;
	data_pages[d+0].pwm_length = 32;
	data_pages[d+0].gamma = 2;
	data_pages[d+1].serial = s;
	data_pages[d+1].page = 1;
	data_pages[d+1].pwm_length = 32;
	data_pages[d+1].gamma = 2;
	data_pages[d+2].serial = s;
	data_pages[d+2].page = 2;
	data_pages[d+2].pwm_length = 32;
	data_pages[d+2].gamma = 2;
	
	for (uint32_t y = 0; y < 32; y++) {
		for (uint32_t x = 0; x < 32; x++) {
			uint32_t offset = get_offset(x+p,y+p);
			uint32_t page = offset / page_size;
			uint8_t *data = data_pages[d+page].data;
			offset -= page * page_size;
			data[offset+2] = (x * 32) / 32;
			data[offset+1] = (y * 32) / 32;
			data[offset+0] = ((63 - x - y)/2 * 32) / 32;
		}
	}
	d += 3;
	d = d % PAGE_COUNT;
	p++;
	s++;
}

struct dma_ctrl {
	uint32_t cfg;
	uint32_t src_end;
	uint32_t dst_end;
	uint32_t link;	
};

static dma_ctrl *dma = reinterpret_cast<dma_ctrl *>(0x10001E00);

int main(void) {

	NVIC_DisableIRQ( DMA_IRQn );
	
	/* Pin Assign 8 bit Configuration */
	/* SPI0_SCK */
	LPC_SWM->PINASSIGN[3] = 0x06ffffffUL;
	/* SPI0_MOSI */
	/* SPI0_SSEL0 */
	LPC_SWM->PINASSIGN[4] = 0xff08ff07UL;

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

	/* Enable SPI clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<11);

	/* Bring SPI out of reset */
	LPC_SYSCON->PRESETCTRL &= ~(0x1<<0);
	LPC_SYSCON->PRESETCTRL |= (0x1<<0);

	/* Set clock speed and mode */
	LPC_SPI0->DIV = 100;
	LPC_SPI0->DLY = 0;
	LPC_SPI0->CFG = ((1UL << 2) & ~(1UL << 0));
	LPC_SPI0->CFG |= (1UL << 0);

	// Enable DMA peripheral clock at the rate of the AHB clock
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 29);

	LPC_DMA->SRAMBASE = reinterpret_cast<uint32_t>(dma); // Point DMA control to the dmaChannel1 structure 
	LPC_DMA->CTRL = (1UL << 0); // Enable the DMA controller
	LPC_DMA->ENABLESET0 = (1UL << 0); // Enable DMA channel 0

	LPC_DMA->CFG0 = 
		(1UL << 0) | // Peripheral request Enable
		(1UL << 1) | // HW trigger Enable
		(1UL << 4) | // Trigger polarity (0:Active low - falling edge) (1:Active high - falling edge)
		(0UL << 5) | // Trigger type (0: Edge) (1: Level)
		(0UL << 6) ; // Trigger burst mode

	// Set up reload configurations
	uint32_t channel_cfg0 = 
		(1UL <<  0) | // Configuration valid
		(1UL <<  1) | // Reload
		(0UL <<  2) | // software trigger
		(0UL <<  3) | // clear trigger
		(0UL <<  4) | // set interrupt A
		(0UL <<  5) | // set interrupt B
		(1UL <<  8) | // 16bit transfer
		(0UL << 12) | // 0x src increment
		(1UL << 14) | // 1x dst increment
		(((sizeof(data_page)/2)-1) << 16); // set transfer size

	// Set up DMA transfer chain
	for (uint32_t c = 0; c < 6; c++) {
		dma[c].cfg = channel_cfg0;
		dma[c].src_end = reinterpret_cast<uint32_t>(&LPC_SPI0->RXDAT);
		dma[c].dst_end = reinterpret_cast<uint32_t>(&data_pages[c+1]);
		dma[c].link = reinterpret_cast<uint32_t>(&dma[c+1]);
	}

	LPC_DMA->XFERCFG0 = channel_cfg0;
	
	if (((uint8_t *)&data_pages[6]) >= ((uint8_t *)dma)) {
		for ( ;; ) {
			static uint32_t led_blink_count = 0;
			if ((led_blink_count++ & 0x0800)) {
				LPC_GPIO_PORT->SET0 = (1<<LED1);
			} else {
				LPC_GPIO_PORT->CLR0 = (1<<LED1);
			}
		}
	}
	
	// Start program
	LPC_GPIO_PORT->SET0 = (1<<LED0);

	uint32_t gamma = 1;
	gradient_test();
	LPC_GPIO_PORT->CLR0 = (1<<LED0);
	while(1) {
		gradient_test();
    	output_frame();
    	gamma += 1;
    }

    return 0;
}

