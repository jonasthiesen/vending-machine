#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>     
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>

MCUFRIEND_kbv tft;
#define SD_CS 5

#if defined(__SAM3X8E__)
#undef __FlashStringHelper::F(string_literal)
#define F(string_literal) string_literal
#endif

File root;
char namebuf[32] = "select.bmp";
int pathlen;
uint8_t spi_save;
uint8_t YP = A1;  // must be an analog pin, use "An" notation!
uint8_t XM = A2;  // must be an analog pin, use "An" notation!
uint8_t YM = 7;   // can be a digital pin
uint8_t XP = 6;   // can be a digital pin
uint8_t SwapXY = 0;

uint16_t TS_LEFT = 880;
uint16_t TS_RT  = 170;
uint16_t TS_TOP = 950;
uint16_t TS_BOT = 180;
char *name = "Unknown controller";

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 250);
TSPoint tp;

#define MINPRESSURE 20
#define MAXPRESSURE 1000

#define SWAP(a, b) { uint16_t tmp = a; a = b; b = tmp; }

int16_t BOXSIZE;
int16_t PENRADIUS = 3;
uint16_t identifier, oldcolor, currentcolor;
uint8_t Orientation = 0; //PORTRAIT

int mode = 1;
int selection;

uint16_t ID;
uint16_t tmp;

void setup() {
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  pinMode(XP, OUTPUT);
  pinMode(YM, OUTPUT);
  
  Serial.begin(9600);
  Serial.print("Show BMP files on TFT with ID:0x");
  ID = tft.readID();
  Serial.println(ID, HEX);
  if (ID == 0x0D3D3) ID = 0x9481;
  tft.begin(ID);
  tft.fillScreen(0x0000);

  // constructor
  ts = TouchScreen(XP, YP, XM, YM, 300);
  
  bool good = SD.begin(SD_CS);
  if (!good) {
    Serial.print(F("cannot start SD"));
    while (1);
  }
  
  tft.setRotation(2);
  bmpDraw("select.bmp", 5, -32);
}

void selectProduct(int value) {
  if (value > 775) {
    
      selection = 1;
      
  } else if (value > 570) {
    
      selection = 2;
      
  } else if (value > 350) {
      
      selection = 3;
      
  } else if (value > 0) {
      
      selection = 4;
  }

  drawImage("payment.bmp");
}

void selectPaymentOption(int value) {
  if (value > 570) {
    drawImage("failure.bmp");
  } else if (value > 0) {
    drawImage("nfc.bmp");
  }
}

void drawImage(char *image) {
  tft.begin(ID);
  tft.fillScreen(0x0000);
  bmpDraw(image, 5, 0);
}

void showStatus(bool success) {
  if (success) {
    drawImage("success.bmp");
  } else {
    drawImage("failure.bmp");
  }
}

void loop()
{
  tp = ts.getPoint();
      
  if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
    switch(mode) {
      case 1:
        selectProduct(tp.y);
        mode = 2;
        break;

      case 2:
        selectPaymentOption(tp.y);
        mode = 3;
        break;
    }
  }
  
  switch(mode) {
    case 0:
      drawImage("select.bmp");
      mode = 1;
      break;

    case 3:
      delay(3000);
      showStatus(true);
      delay(3000);
      mode = 0;
      break;
  }
}

#define BUFFPIXEL 20

void bmpDraw(char *filename, int x, int y) {
  File bmpFile;
  int bmpWidth, bmpHeight;
  uint8_t	bmpDepth;
  uint32_t bmpImageoffset;
  uint32_t rowSize;
  uint8_t	sdbuffer[3 * BUFFPIXEL];
  uint16_t lcdbuffer[BUFFPIXEL];
  uint8_t	buffidx = sizeof(sdbuffer);
  boolean	goodBmp = false;
  boolean	flip	= true;
  int w, h, row, col;
  uint8_t	r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t	lcdidx = 0;
  boolean	first = true;

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');
  
  SPCR = spi_save;
  
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }
  
  if (read16(bmpFile) == 0x4D42) {
    Serial.print(F("File size: "));
    Serial.println(read32(bmpFile));
    
    (void)read32(bmpFile);
    bmpImageoffset = read32(bmpFile);
    
    Serial.print(F("Image Offset: "));
    Serial.println(bmpImageoffset, DEC);

    Serial.print(F("Header size: "));
    Serial.println(read32(bmpFile));
    
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    
    if (read16(bmpFile) == 1) {
      bmpDepth = read16(bmpFile);
      
      Serial.print(F("Bit Depth: "));
      Serial.println(bmpDepth);
      
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) {
        goodBmp = true;
        
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);
        
        rowSize = (bmpWidth * 3 + 3) & ~3;

        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip 	 = false;
        }

        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;
        
        SPCR = 0;
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) {
          if (flip) {
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          } else {
            pos = bmpImageoffset + row * rowSize;
          }
          
          SPCR = spi_save;
          
          if (bmpFile.position() != pos) {
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer);
          }

          for (col = 0; col < w; col++) {
            if (buffidx >= sizeof(sdbuffer)) {
              if (lcdidx > 0) {
                SPCR	= 0;
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first	= false;
              }
              
              SPCR = spi_save;
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0;
            }

            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r, g, b);
          }
        }
        
        if (lcdidx > 0) {
          SPCR = 0;
          tft.pushColors(lcdbuffer, lcdidx, first);
        }
        
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      }
    }
  }

  bmpFile.close();
  if (!goodBmp) Serial.println("BMP format not recognized.");
}

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
