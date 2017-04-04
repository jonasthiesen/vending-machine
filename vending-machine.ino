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
char namebuf[32] = "payment.bmp";
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

#define SWAP(a, b) {uint16_t tmp = a; a = b; b = tmp;}

int16_t BOXSIZE;
int16_t PENRADIUS = 3;
uint16_t identifier, oldcolor, currentcolor;
uint8_t Orientation = 0;    //PORTRAIT

int mode = 1;
int selection;

void setup() {
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  pinMode(XP, OUTPUT);
  pinMode(YM, OUTPUT);
  
  uint16_t ID;
  uint16_t tmp;
  
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
  bmpDraw("payment.bmp", 0, -32);
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
}

void loop()
{
  tp = ts.getPoint();
      
  if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) {
    switch(mode) {
      case 1:
        selectProduct(tp.y);
        mode = 2;
        bmpDraw("payment.bmp", 0, -32);
        break;

      case 2:
        Serial.println("here");
        break;
        
      default:
        Serial.println("fak");
        break;
    }
  }
}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.	20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw(char *filename, int x, int y) {
  File 	bmpFile;
  int		bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t	bmpDepth;			   // Bit depth (currently must be 24)
  uint32_t bmpImageoffset; 	   // Start of image data in file
  uint32_t rowSize;			   // Not always = bmpWidth; may have padding
  uint8_t	sdbuffer[3 * BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
  uint8_t	buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean	goodBmp = false;	   // Set to true on valid header parse
  boolean	flip	= true; 	   // BMP is stored bottom-to-top
  int		w, h, row, col;
  uint8_t	r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t	lcdidx = 0;
  boolean	first = true;

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');
  // Open requested file on SD card
  SPCR = spi_save;
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip 	 = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        SPCR = 0;
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) { // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else 	// Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          SPCR = spi_save;
          if (bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col = 0; col < w; col++) { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to the display first
              if (lcdidx > 0) {
                SPCR	= 0;
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first	= false;
              }
              SPCR = spi_save;
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r, g, b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if (lcdidx > 0) {
          SPCR = 0;
          tft.pushColors(lcdbuffer, lcdidx, first);
        }
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp) Serial.println("BMP format not recognized.");
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

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

