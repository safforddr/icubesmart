/*
 * A very simple C program for an 8x8x8 monochrome cube such as icubesmart 3D8S
 * or possibly others with compatible stc12/8051 based controllers.
 *
 * Based on tomazas 888.c on github, but ported for the 3D8S and SDCC/Linux.
 * it also supports the 3D8S's three buttons for pause and mode selection.
 * it also shows how to scroll your name easily.
 *
 * Compile with sdcc-sdcc --std-c99 -mmcs51 dave.c
 * translate to hex with sdcc-packihx dave.ihx > dave.hex
 * Program the controller with stcgal -P stc12 -p /dev/ttyUSB0 dave.hex
 * Note: I had to switch the ground rather than 5V (from the USB-TTY
 * device to the cube) because of parasitic drain. Read the stcgal
 * documentation for details.
 *
 * Looking from the front (LEDs all point to the front) the cube axes are:
 *     X is left to right
 *     Y is front to back
 *     Z is bottom to top
 */

#include <stc12.h>

/* led state storage for the entire cube.
 * (Each 'x' is the 8 bit value in the display[z][y] array,
 * most significant bit on the left.)
 * For each bit, a '0' sinks or turns on the led.
 * This is kept in the extended ram due to its size.
 */
__xdata volatile unsigned char display[8][8];

static int mode = 0;

/*
 ********** timing, interrrupt and I/O functions ***********************
 */

/* configure and start interrupts */
void sinter(void)
{
	IE = 0x82;	// EA | ET0
	TCON = 0x01;	// IT0
	TH0 = 0xc0;	// timer 0 high byte ~.05 sec
	TL0 = 0;	// timer 0 low byte
	TR0 = 1;	// Turn on the timer
}

/* simple software timer */
void delay5us(void)
{
	unsigned char a, b;
	for(b=7; b>0; b--)
		for(a=2; a>0; a--)
			;
}

/* simple longer delay for main or interrupt threads */
void delay(unsigned int i)
{
	while (i--) {
		delay5us();
	}
}

/* Main thread software delay with support for pause and mode buttons.
 *
 * Button 1, 2, and 3 are connected to P4_1, P4_2, and P4_3, active low.
 * Button 1 is pause on/off
 * Button 2 selects mode 0
 * Button 3 sequences through modes one at a time
 *
 * We poll the buttons in here for simplicity and fast response.
 * Pause takes effect immediately, and mode changes return true
 * immediately to the caller for handling there.
 */
int pause(unsigned int i)
{
	static unsigned char pause = 0, p41key = 0, p42key = 0, p43key = 0;;

	while (i--) {
		delay5us();

		/* handle pause (button 1) */
		while (1) {
			if (!P4_1){			// button 1 pressed
				if (!p41key){		// new press
					p41key = 1;
					pause = !pause; // toggle pause
					delay(100); 	//debounce
				}
			} else
				p41key = 0;
			if (!pause)	// stay in loop until unpaused
				break;
		}

		/* handle button 2 (set mode to zero) */
		if (!P4_2)
			if (!p42key) {
				p42key = 1;
				mode = 0;
				delay(100); //debounce
				return(1);
			}
		else
			p42key = 0;

		/* handle button 3 (increment mode) */
		if (!P4_3){
			if (!p43key) {
				p43key = 1;
				mode = (mode +1) % 5;
				delay(100); //debounce
				return(1);
			}
		} else
			p43key = 0;
	}
	return 0;
}

/*
 ****************** display primitives ************************
 */

/* turn all leds in the whole cube on or off */
void set_all(unsigned char v)
{
	unsigned char i, j;
	for (j=0; j<8; j++) {
		for (i=0; i<8; i++)
			display[j][i] = v ? 0 : 0xff;
	}
}

/* turn all leds in an x plane on or off */
void set_x_plane(unsigned char x, unsigned char v)
{
	unsigned char i, j;
	for (j=0; j<8; j++) {
		for (i=0; i<8; i++)
			if (v)
				display[j][i] &= ~(128>>x);
			else
				display[j][i] |= (128>>x);
	}
}

/* turn all leds in an y plane on or off */
void set_y_plane(unsigned char y, unsigned char v)
{
	unsigned char j;
	for (j=0; j<8; j++) {
		if (v)
			display[j][y] = 0;
		else
			display[j][y] = 0xff;
	}
}

/* turn all leds in an z plane on or off */
void set_z_plane(unsigned char z, unsigned char v)
{
	unsigned char i;
	for (i=0; i<8; i++) {
		if (v)
			display[z][i] = 0;
		else
			display[z][i] = 0xff;
	}
}

void set_point(unsigned char x, unsigned char y, unsigned char z,
	       unsigned char v)
{
	if (v)
		display[z][y] &= ~(128>>x);
	else
		display[z][y] |= (128>>x);
}

/* display a 64 led y plane from (positive logic bit array */
void character_on_y(unsigned char y, unsigned char *c)
{
	int z;
	for (z=0; z<8; z++)
		display[z][y] = ~c[z]; //make negative
}

/*
 ******************** display routines ***********************
 */

 /* blink the whole cube 
  * Return (1) if pause indicates a button pressed 
  */
int all(void)
{
	/* turn on all leds */
	set_all(1);
	if (pause(60000))
		return(1);

	/* turn off all leds */
	set_all(0);
	if (pause(60000))
		return(1);
	return(0);
}

/* sweep the x, y, and z planes 
 * Return (1) if pause indicates a button pressed 
 */
int planes(void)
{
	int x, y, z;

	set_all(0);
	/* cycle through all planes */
	for (x=0; x<8; x++){
		set_x_plane(x, 1);
		if (pause(10000))
			return(1);
		set_x_plane(x, 0);
	}

	for (y=0; y<8; y++){
		set_y_plane(y, 1);
		if (pause(10000))
			return(1);
		set_y_plane(y, 0);
	}

	for (z=0; z<8; z++){
		set_z_plane(z, 1);
		if (pause(10000))
			return(1);
		set_z_plane(z, 0);
	}
	return(0);
}

/* sweep each individual led in the whole cube 
 * Return (1) if pause indicates a button pressed 
 */
int points(void)
{
	int x, y, z;
	
	set_all(0);
	for (x=0; x<8; x++) {
		for (y=0; y<8; y++) {
			for (z=0; z<8; z++) {
				set_point(x,y,z,1);
				if (pause(1000))
					return(1);
				set_point(x,y,z,0);
			}
		}
	}
	return(0);
}

/* display "DAVE" scrolled on the y planes front to back 
 * Return (1) if pause indicates a button pressed 
 */
int dave(void)
{
	/* Bit mapped characters for "DAVE".
	 * These are positive logic ('1' means on) 
	 * Since this is read-only, keep in text space to save RAM
	 */
	__code unsigned char dave[4][8] = {
		{0xf8, 0xfc, 0xc6, 0xc3, 0xc3, 0xc6, 0xfc, 0xf8},	// D
		{0xc3, 0xc3, 0xff, 0xff, 0xc3, 0x66, 0x3c, 0x18},	// A
		{0x18, 0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3},	// V
		{0xff, 0xff, 0xc0, 0xf8, 0xf8, 0xc0, 0xff, 0xff}	// E
	};
	int c, y;

	set_all(0);
	for (c=0; c<4; c++) {
		for (y=0; y<8; y++) {
			character_on_y(y, dave[c]);
			if (pause(5000))
				return(1);
			set_y_plane(y, 0);
		}
	}
	set_all(1);
	if (pause(30000))
		return(1);
	return(0);
}

/* initialize and loop display routines forever */
void main()
{
	sinter();  // turn on interrupts
	P4 = 0xff; // enable button input port

	while(1) {
		switch (mode) {
			case 0:
				dave();
				break;
			case 1:
				points();
				break;
			case 2:
				planes();
				break;
			case 3:
				all();
				break;
			case 4:
				if(dave())
					break;;
				if(points())
					break;
				if(planes())
					break;
				all();
				break;
		}
	}
}

/*
 * The timer interrupt service routine.
 *
 * On each interrupt, we turn on the next whole anode (Z) layer,
 * and it remains on until the next timer interrupt.
 * Each layer is on only one out of eight interrupts,
 * but the multiplexing is fast enough that the eye
 * does not see any flickering.
 *
 * For each layer, the latches hold the bits for all 64
 * vertical cathode lines, so each led in the layer is
 * individually controlled.
 *
 * P1 selects the horizontal anode layer (one at a time)
 *    NOTE! icubesmart 3D8S uses inverted drivers, so 1 is off.
 * P2 selects a cathode latch chip to load a new data byte.
 * P0 sets the cathode latch data (a '1' bit turns off the LED).
 *
 * The timing in here is sensitive, so don't add a lot.
 */
void timer_isr (void) __interrupt (1)
{
	static unsigned char layer = 0;
	unsigned char i;

	/* turn off all layers while loading new data */
	P1 = 255;

	/* load the 8 bytes for this layer into latches */
	for (i=0; i<8; i++) {
		P2 = 1<<i;		// select latch
		delay(3);
		P0 = display[layer][i]; // write its data byte
		delay(3);
	}

	/* turn on this layer */
	P1 = ~(1<<layer);

	/* increment layer for next time */
	layer++;
	if (layer > 7)
		layer = 0;

	/* reload timer */
	TH0 = 0xc0;
	TL0 = 0;
}
