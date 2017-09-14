/*********************************************************************
This is a library for our eInk displays based on EINK drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/TODO

These displays use SPI to communicate, 6 pins are requiEINK_RED to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Dean Miller  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen below must be included in any EINK_REDistribution
*********************************************************************/

#ifdef __AVR__
  #include <avr/pgmspace.h>
#elif defined(ESP8266) || defined(ESP32)
 #include <pgmspace.h>
#else
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

#if !defined(__ARM_ARCH) && !defined(ENERGIA) && !defined(ESP8266) && !defined(ESP32) && !defined(__arc__)
 #include <util/delay.h>
#endif

#include <stdlib.h>

#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_EINK.h"

#ifndef USE_EXTERNAL_SRAM
// the memory buffer for the LCD
uint16_t buffer[EINK_BUFSIZE];
#else

#define RAMBUFSIZE 64

#endif

// the most basic function, set a single pixel

#ifdef USE_EXTERNAL_SRAM
Adafruit_EINK::Adafruit_EINK(int8_t SID, int8_t SCLK, int8_t DC, int8_t RST, int8_t CS, int8_t BUSY, int8_t SRCS, int8_t MISO) : Adafruit_GFX(EINK_LCDWIDTH, EINK_LCDHEIGHT),
sram(SID, MISO, SCLK, SRCS) {
#else
Adafruit_EINK::Adafruit_EINK(int8_t SID, int8_t SCLK, int8_t DC, int8_t RST, int8_t CS, int8_t BUSY) : Adafruit_GFX(EINK_LCDWIDTH, EINK_LCDHEIGHT) {
#endif
  cs = CS;
  rst = RST;
  dc = DC;
  sclk = SCLK;
  sid = SID;
  busy = BUSY;
  hwSPI = false;
}

// constructor for hardware SPI - we indicate DataCommand, ChipSelect, Reset
#ifdef USE_EXTERNAL_SRAM
Adafruit_EINK::Adafruit_EINK(int8_t DC, int8_t RST, int8_t CS, int8_t BUSY, int8_t SRCS) : Adafruit_GFX(EINK_LCDWIDTH, EINK_LCDHEIGHT),
sram(SRCS) {
#else
Adafruit_EINK::Adafruit_EINK(int8_t DC, int8_t RST, int8_t CS, int8_t BUSY) : Adafruit_GFX(EINK_LCDWIDTH, EINK_LCDHEIGHT) {
#endif
  dc = DC;
  rst = RST;
  cs = CS;
  busy = BUSY;
  hwSPI = true;
}


void Adafruit_EINK::begin(bool reset) {
  blackInverted = true;
  redInverted = false;
  
#ifdef USE_EXTERNAL_SRAM
	sram.begin();
	sram.write8(0, K640_SEQUENTIAL_MODE, K640_WRSR);
#endif
  
  // set pin directions
    pinMode(dc, OUTPUT);
    pinMode(cs, OUTPUT);
#ifdef HAVE_PORTREG
    csport      = portOutputRegister(digitalPinToPort(cs));
    cspinmask   = digitalPinToBitMask(cs);
    dcport      = portOutputRegister(digitalPinToPort(dc));
    dcpinmask   = digitalPinToBitMask(dc);
#endif

	csHigh();

    if (!hwSPI){
      // set pins for software-SPI
      pinMode(sid, OUTPUT);
      pinMode(sclk, OUTPUT);
#ifdef HAVE_PORTREG
      clkport     = portOutputRegister(digitalPinToPort(sclk));
      clkpinmask  = digitalPinToBitMask(sclk);
      mosiport    = portOutputRegister(digitalPinToPort(sid));
      mosipinmask = digitalPinToBitMask(sid);
#endif
      }
    if (hwSPI){
      SPI.begin();
#ifdef SPI_HAS_TRANSACTION
      SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
#else
      SPI.setClockDivider (4);
#endif
    }

  if ((reset) && (rst >= 0)) {
    // Setup reset pin direction
    pinMode(rst, OUTPUT);
    digitalWrite(rst, HIGH);
    // VDD (3.3V) goes high at start, lets just chill for a ms
    delay(1);
    // bring reset low
    digitalWrite(rst, LOW);
    // wait 10ms
    delay(10);
    // bring out of reset
    digitalWrite(rst, HIGH);
  }
  pinMode(busy, INPUT);
  while(digitalRead(busy)); //wait for busy low
}

void Adafruit_EINK::EINK_command(uint8_t c, const uint8_t *buf, uint16_t len)
{
	EINK_command(c, false);
	EINK_data(buf, len);
}

void Adafruit_EINK::EINK_command(uint8_t c, bool end) {
    // SPI
	csHigh();
	dcLow();
	csLow();
	
    fastSPIwrite(c);

	if(end){
		csHigh();
	}
}

void Adafruit_EINK::EINK_data(const uint8_t *buf, uint16_t len)
{
	// SPI
	dcHigh();

	for (uint16_t i=0; i<len; i++) {
		fastSPIwrite(buf[i]);
	}
	csHigh();
}

inline void Adafruit_EINK::fastSPIwrite(uint8_t d) {

  if(hwSPI) {
    (void)SPI.transfer(d);
  } else {
    for(uint8_t bit = 0x80; bit; bit >>= 1) {
#ifdef HAVE_PORTREG
      *clkport &= ~clkpinmask;
      if(d & bit) *mosiport |=  mosipinmask;
      else        *mosiport &= ~mosipinmask;
      *clkport |=  clkpinmask;
#else
      digitalWrite(sclk, LOW);
      if(d & bit) digitalWrite(sid, HIGH);
      else        digitalWrite(sid, LOW);
      digitalWrite(sclk, HIGH);
#endif
    }
  }
}

void Adafruit_EINK::display()
{
#ifdef USE_EXTERNAL_SRAM
	uint8_t databuf[RAMBUFSIZE];
	
	for(uint16_t i=0; i<EINK_BUFSIZE*2; i+=RAMBUFSIZE){
		sram.read(i, databuf, RAMBUFSIZE);
		
		//write image
		EINK_command(EINK_RAM_BW, false);
		dcHigh();
		
		uint16_t toWrite = min(EINK_BUFSIZE*2 - i, RAMBUFSIZE);
		for(uint16_t j=1; j<toWrite; j+=2){
			fastSPIwrite(databuf[j]);
		}

		csHigh();
		
	#ifdef EINK_RAM_RED //write red if this is a tricolor display
		EINK_command(EINK_RAM_RED, false);
		dcHigh();
		
		for(uint16_t j=0; j<toWrite; j+=2){
			fastSPIwrite(databuf[j]);
		}

		csHigh();
	#endif
	}
	
#else
	//write image
	EINK_command(EINK_RAM_BW, false);
	dcHigh();

	for(uint16_t i=0; i<EINK_BUFSIZE; i++){
		fastSPIwrite(buffer[i] & 0xFF);
	}
	csHigh();
	
	#ifdef EINK_RAM_RED //write red if this is a tricolor display
	EINK_command(EINK_RAM_RED, false);
	dcHigh();
	
	for(uint16_t i=0; i<EINK_BUFSIZE; i++){
		fastSPIwrite( (buffer[i] >> 8) & 0xFF);
	}
	csHigh();
	#endif //RAM_RED

#endif
}

void Adafruit_EINK::csHigh()
{
#ifdef HAVE_PORTREG
	*csport |= cspinmask;
#else
	digitalWrite(cs, HIGH);
#endif
}

void Adafruit_EINK::csLow()
{
#ifdef HAVE_PORTREG
	*csport &= ~cspinmask;
#else
	digitalWrite(cs, LOW);
#endif
}

void Adafruit_EINK::dcHigh()
{
#ifdef HAVE_PORTREG
	*dcport |= dcpinmask;
#else
	digitalWrite(dc, HIGH);
#endif
}

void Adafruit_EINK::dcLow()
{
#ifdef HAVE_PORTREG
	*dcport &= ~dcpinmask;
#else
	digitalWrite(dc, LOW);
#endif
}

void Adafruit_EINK::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  boolean bSwap = false;
  switch(rotation) {
    case 0:
      // 0 degree rotation, do nothing
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x
      bSwap = true;
      EINK_swap(x, y);
      x = WIDTH - x - 1;
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      x -= (w-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y  and adjust y for w (not to become h)
      bSwap = true;
      EINK_swap(x, y);
      y = HEIGHT - y - 1;
      y -= (w-1);
      break;
  }

  if(bSwap) {
    drawFastVLineInternal(x, y, w, color);
  } else {
    drawFastHLineInternal(x, y, w, color);
  }
}

void Adafruit_EINK::drawFastVLineInternal(int16_t x, int16_t __y, int16_t __h, uint16_t color) {
	  // do nothing if we're off the left or right side of the screen
  if(x < 0 || x >= WIDTH) { return; }

  // make sure we don't try to draw below 0
  if(__y < 0) {
    // __y is negative, this will subtract enough from __h to account for __y being 0
    __h += __y;
    __y = 0;

  }

  // make sure we don't go past the height of the display
  if( (__y + __h) > HEIGHT) {
    __h = (HEIGHT - __y);
  }

  // if our height is now negative, punt
  if(__h <= 0) {
    return;
  }

  // set up the pointer for  movement through the buffer
  uint16_t addr = (x * EINK_LCDHEIGHT / 8) + __y/8;
  
#ifdef USE_EXTERNAL_SRAM
  addr = addr * 2; //2 bytes each in sram
  uint16_t * pBuf;
  uint16_t c = sram.read16(addr);
  pBuf = &c;
#else
  register uint16_t *pBuf = buffer;
  // adjust the buffer pointer for the current row
  pBuf += addr;
#endif
  
  uint8_t blocks = __h/8;
  while(blocks){
	  	switch (color)
	  	{
		  	case EINK_BLACK:         *pBuf = 0xFF; break;
		  	case EINK_WHITE:			*pBuf = 0x00; break;
		  	case EINK_INVERSE:       *pBuf ^= 0xFF; break;
		  	case  EINK_RED:			*pBuf = 0xFF00; break;
	  	}
#ifdef USE_EXTERNAL_SRAM
	  //write the new value
	  sram.write16(addr, *pBuf);
	  
	  //increment the addres
	  addr =+ 2;
	  *pBuf = sram.read16(addr);
#endif
	  blocks--;
  }
  
  //do the last block w/ the leftover
  for(uint16_t i=0; i< __h%8; i++){
	  switch (color)
		{
		  case EINK_BLACK:   *pBuf |= (1 << (7 - i&7)); break;
		  case EINK_WHITE:   *pBuf  &= ~(1 << (7 - i&7)); break;
		  case EINK_INVERSE: *pBuf  ^= (1 << (7 - i&7)); break;
		  case EINK_RED:   *pBuf  |= (1 << (15 - (i%8))); break;
		}
  }
}

void Adafruit_EINK::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  bool bSwap = false;
  switch(rotation) {
    case 0:
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x and adjust x for h (now to become w)
      bSwap = true;
      EINK_swap(x, y);
      x = WIDTH - x - 1;
      x -= (h-1);
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      y -= (h-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y
      bSwap = true;
      EINK_swap(x, y);
      y = HEIGHT - y - 1;
      break;
  }

  if(bSwap) {
    drawFastHLineInternal(x, y, h, color);
  } else {
    drawFastVLineInternal(x, y, h, color);
  }
}


void Adafruit_EINK::drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color) {

    // Do bounds/limit checks
  if(y < 0 || y >= HEIGHT) { return; }

  // make sure we don't try to draw below 0
  if(x < 0) {
    w += x;
    x = 0;
  }

  // make sure we don't go off the edge of the display
  if( (x + w) > WIDTH) {
    w = (WIDTH - x);
  }

  // if our width is now negative, punt
  if(w <= 0) { return; }
	  
  uint16_t addr = (x * EINK_LCDHEIGHT / 8) + y/8;

#ifdef USE_EXTERNAL_SRAM
	addr = addr * 2; //2 bytes in sram
	uint16_t * pBuf;
	uint16_t c = sram.read16(addr * 2);
	pBuf = &c;
#else
    register uint16_t *pBuf = buffer;
	pBuf = buffer + addr;
#endif
	// x is which column
	switch (color)
	{
		case EINK_BLACK:   *pBuf |= (1 << (7 - y&7)); break;
		case EINK_WHITE:   *pBuf &= ~(1 << (7 - y&7)); break;
		case EINK_INVERSE: *pBuf ^= (1 << (7 - y&7)); break;
		case EINK_RED:   *pBuf |= (1 << (15 - (y%8))); break;
	}
#ifdef USE_EXTERNAL_SRAM
	sram.write16(addr, *pBuf);
#endif
  
  uint8_t i = (y%8);
  for(uint16_t j=0; j< w; j++){
	switch (color)
	{
		case EINK_BLACK:   *pBuf |= (1 << (7 - i)); break;
		case EINK_WHITE:   *pBuf  &= ~(1 << (7 - i)); break;
		case EINK_INVERSE: *pBuf  ^= (1 << (7 - i)); break;
		case EINK_RED:   *pBuf  |= (1 << (15 - i)); break;
	}
#ifdef USE_EXTERNAL_SRAM
	//write the new value
	sram.write16(addr, *pBuf);

	//increment the addres
	addr += (EINK_LCDHEIGHT / 8) * 2;
	
	*pBuf = sram.read16(addr);
#else
	*pBuf += EINK_LCDHEIGHT / 8;
#endif
  }
}
