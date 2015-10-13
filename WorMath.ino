#include <SPI.h>

// Flag that puts code in debug mode
//#define DEBUG

#define LCD_WIDTH 84
#define LCD_HEIGHT 48

// Defines the screen boundaries for the snake
#define LCD_RIGHT LCD_WIDTH - 10
#define LCD_LEFT 0
#define LCD_TOP 0
#define LCD_BOTTOM LCD_HEIGHT

// Used for communication with the LCD screen
#define LCD_COMMAND 0
#define LCD_DATA 1

// Side length of a snake segment
#define SNAKE_WIDTH 2
// Maximum number of snake segments
#define MAX_SNAKE_LENGTH 50

// Maximum number of numbers
#define MAX_SUMS 3

// Byte representations for some characters that will be drawn to the screen
const byte characters[][5] = {
  {0x3e, 0x51, 0x49, 0x45, 0x3e} // 0x30 0
  ,{0x00, 0x42, 0x7f, 0x40, 0x00} // 0x31 1
  ,{0x42, 0x61, 0x51, 0x49, 0x46} // 0x32 2
  ,{0x21, 0x41, 0x45, 0x4b, 0x31} // 0x33 3
  ,{0x18, 0x14, 0x12, 0x7f, 0x10} // 0x34 4
  ,{0x27, 0x45, 0x45, 0x45, 0x39} // 0x35 5
  ,{0x3c, 0x4a, 0x49, 0x49, 0x30} // 0x36 6
  ,{0x01, 0x71, 0x09, 0x05, 0x03} // 0x37 7
  ,{0x36, 0x49, 0x49, 0x49, 0x36} // 0x38 8
  ,{0x08, 0x08, 0x3e, 0x08, 0x08} // 0x2b +
};

// Structure that represents a segment of the snake
struct Segment {
  int x;
  int y;
  int dir;
};

// Structure that reperesents a possible sum on the screen
struct Sum {
  int x;
  int y;
  int num;
};

// Array of blocks that make up the snake
Segment segments[MAX_SNAKE_LENGTH];
// Length of the snake in segments
int snakeLength = 1;

// Switch that determines if the game is over
bool endGame = false;
// Switch that determines if the expression must be reset
bool resetExpr = false;

// Holds the current arithmetic problem
struct {
  int oppA;
  int oppB;
} expression;
Sum sums[MAX_SUMS];

// Number dimensions for collision purposes
#define NUM_WIDTH 5
#define NUM_HEIGHT 7

// Represents the cardinal directions and the pin inputs for the switches
#define RIGHT 2
#define DOWN 8
#define LEFT 3
#define UP 4

// Routine of assignments that resets the snake's position and length
#define RESET_SNAKE \
segments[0].x = 40; \
segments[0].y = 24; \
segments[0].dir = UP; \
snakeLength = 3; \
for(int i = 1; i < snakeLength; ++i) { \
  segments[i].x = segments[0].x - 2 * i; \
  segments[i].y = segments[0].y; \
  segments[i].dir = RIGHT; \
}

// Routine of assignments that generates a new arithmetic problem and random answers
#define RESET_EXPRESSION \
expression.oppA = random(5); \
expression.oppB = random(5); \
sums[0].num = expression.oppA + expression.oppB; \
sums[0].x = random(LCD_LEFT + 1, LCD_RIGHT - 5); \
sums[0].y = random(LCD_TOP + 1, LCD_BOTTOM - 7); \
int i = 1; \
while(i < MAX_SUMS) { \
  sums[i].x = random(LCD_LEFT + 1, LCD_RIGHT - 5); \
  sums[i].y = random(LCD_TOP + 1, LCD_BOTTOM - 7); \
  sums[i].num = random(8); \
  ++i; \
}

// Consider changing to macros
#define SCE_PIN 7 // SCE - Chip select, pin 3 on LCD.
#define RST_PIN 6 // RST - Reset, pin 4 on LCD.
#define DC_PIN 5 // DC - Data/Command, pin 5 on LCD.
#define SDIN_PIN 11 // DN(MOSI) - Serial data, pin 6 on LCD.
#define SCLK_PIN 13 // SCLK - Serial clock, pin 7 on LCD.
#define BL_PIN 9 // LED - Backlight LED, pin 8 on LCD.

// Holds the bytes that will be sent to the LCD for display
byte displayMap[LCD_WIDTH * LCD_HEIGHT / 8] = {0};

// Initializes the LCD pins
void lcdBegin(void) 
{
  //Configure control pins
  pinMode(SCE_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(DC_PIN, OUTPUT);
  pinMode(SDIN_PIN, OUTPUT);
  pinMode(SCLK_PIN, OUTPUT);
  pinMode(BL_PIN, OUTPUT);
  analogWrite(BL_PIN, 255);

  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  
  //Reset the LCD to a known state
  digitalWrite(RST_PIN, LOW);
  digitalWrite(RST_PIN, HIGH);

  LCDWrite(LCD_COMMAND, 0x21); //Tell LCD extended commands follow
  LCDWrite(LCD_COMMAND, 0xB0); //Set LCD Vop (Contrast)
  LCDWrite(LCD_COMMAND, 0x04); //Set Temp coefficent
  LCDWrite(LCD_COMMAND, 0x14); //LCD bias mode 1:48 (try 0x13)
  //We must send 0x20 before modifying the display control mode
  LCDWrite(LCD_COMMAND, 0x20); 
  LCDWrite(LCD_COMMAND, 0x0C); //Set display control, normal mode.
}

// Set contrast can set the LCD Vop to a value between 0 and 127.
// 40-60 is usually a pretty good range.
void setContrast(byte contrast)
{  
  LCDWrite(LCD_COMMAND, 0x21); //Tell LCD that extended commands follow
  LCDWrite(LCD_COMMAND, 0x80 | contrast); //Set LCD Vop (Contrast): Try 0xB1(good @ 3.3V) or 0xBF if your display is too dark
  LCDWrite(LCD_COMMAND, 0x20); //Set display mode
}

// Sets a pixel to be white or black
void setPixel(int x, int y, boolean bw)
{
  // First, double check that the coordinate is in range.
  if ((x >= 0) && (x < LCD_WIDTH) && (y >= 0) && (y < LCD_HEIGHT))
  {
    byte shift = y % 8;
  
    if (bw) // If black, set the bit.
      displayMap[x + (y/8)*LCD_WIDTH] |= 1<<shift;
    else   // If white clear the bit.
      displayMap[x + (y/8)*LCD_WIDTH] &= ~(1<<shift);
  }
}

// Write a command or data to the LCD
void LCDWrite(byte data_or_command, byte data) 
{
  //Tell the LCD that we are writing either to data or a command
  digitalWrite(DC_PIN, data_or_command); 

  //Send the data
  digitalWrite(SCE_PIN, LOW);
  SPI.transfer(data); //shiftOut(SDIN_PIN, SCLK_PIN, MSBFIRST, data);
  digitalWrite(SCE_PIN, HIGH);
}

void gotoXY(int x, int y)
{
  LCDWrite(LCD_COMMAND, 0x80 | x);  // Column.
  LCDWrite(LCD_COMMAND, 0x40 | y);  // Row.  ?
}

// Write the display buffer to the actual LCD
void updateDisplay()
{
  gotoXY(0, 0);
  for (int i=0; i < (LCD_WIDTH * LCD_HEIGHT / 8); i++)
  {
    LCDWrite(LCD_DATA, displayMap[i]);
  }
}

// Clear the display buffer to white
void clearDisplay() {
  for(int i = 0; i < LCD_WIDTH; ++i) {
    for(int j = 0; j < LCD_HEIGHT; ++j) {
      setPixel(i, j, 0);
    }
  }
}

// Draw a rectange (filled)
void rect(int x, int y, int w, int h, int color){
   for(int i = 0; i < w; ++i) {
    for(int j = 0; j < h; ++j) {
      setPixel(i + x, j + y, color);
    }
  }
}

// Print out a character from the array defined above
void setChar(int character, int x, int y, boolean bw)
{
  byte column; // temp byte to store character's column bitmap
  for (int i=0; i<5; i++) // 5 columns (x) per character
  {
    column = characters[character][i];
    for (int j=0; j<8; j++) // 8 rows (y) per character
    {
      if (column & (0x01 << j)) // test bits to set pixels
        setPixel(x+i, y+j, bw);
      else
        setPixel(x+i, y+j, !bw);
    }
  }
}

// Set the entire display buffer to specific data
void drawBitmap(byte* data) {
  for(int i = 0; i < LCD_WIDTH * LCD_HEIGHT / 8; ++i) {
    displayMap[i] = data[i];
  }
}

// Initialize pins and variables
void setup() {
  // put your setup code here, to run once

  byte startScreen[LCD_WIDTH * LCD_HEIGHT / 8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x01, 0x01,
    0x03, 0x07, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFC, 0xF8,
    0xE0, 0xE0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xC0, 0xC0,
    0xC0, 0xC0, 0xE0, 0xF0, 0xF0, 0xF8, 0xFC, 0xFC, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x80, 0xFF, 0xFF, 0x7F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0xCF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFE, 0xFC, 0xF8, 0xF0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x38, 0x3F, 0x3F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00,
    0x00, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x03, 0x01, 0x81, 0xF8, 0xFE,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xE0, 0xF0, 0xF8, 0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFC, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x7C, 0x7E, 0x7F, 0x7F, 0x7F, 0x7F, 0x01,
    0x00, 0x00, 0x00, 0x00, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0xE0, 0xF0, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x07,
    0x07, 0x0F, 0x0F, 0x1F, 0x1F, 0x1F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x1F, 0x1F,
    0x1F, 0x0C, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xE0, 0xE0, 0xE0,
    0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F, 0x1F, 0x1F, 0x3F, 0x7F,
    0xFF, 0x7F, 0x7F, 0x3F, 0x3F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x3F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x07, 0x03, 0x03, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  randomSeed(analogRead(A1));
  
  RESET_SNAKE;
  RESET_EXPRESSION;
  
  pinMode(RIGHT, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(UP, INPUT);
  pinMode(LEFT, INPUT);
  lcdBegin();
  setContrast(50);
  clearDisplay();
#ifndef DEBUG
  drawBitmap(startScreen);
  updateDisplay();
  delay(3000);
#endif
  clearDisplay();
}

// Check if the head of the snake collides with its tail
bool checkSegmentCollisions() {
  for(int i = 1; i < snakeLength; ++i) {
    if(segments[0].x == segments[i].x && segments[0].y == segments[i].y) {
      return true;
    }
  }
  return false;
}

// Move the following snake segments
void moveFollowingSegment(int segNumber) {
  if(segNumber > 0) {
    switch(segments[segNumber].dir) {
    case RIGHT:
      segments[segNumber].x += SNAKE_WIDTH;
      if(segments[segNumber].y < segments[segNumber - 1].y) {
        segments[segNumber].dir = DOWN;
      } else if(segments[segNumber].y > segments[segNumber - 1].y) {
        segments[segNumber].dir = UP;
      }
      break;
    case DOWN:
      segments[segNumber].y += SNAKE_WIDTH;
      if(segments[segNumber].x < segments[segNumber - 1].x) {
        segments[segNumber].dir = RIGHT;
      } else if(segments[segNumber].x > segments[segNumber - 1].x) {
        segments[segNumber].dir = LEFT;
      }
      break;
    case LEFT:
      segments[segNumber].x -= SNAKE_WIDTH;
      if(segments[segNumber].y < segments[segNumber - 1].y) {
        segments[segNumber].dir = DOWN;
      } else if(segments[segNumber].y > segments[segNumber - 1].y) {
        segments[segNumber].dir = UP;
      }
      break;
    case UP:
      segments[segNumber].y -= SNAKE_WIDTH;
      if(segments[segNumber].x < segments[segNumber - 1].x) {
        segments[segNumber].dir = RIGHT;
      } else if(segments[segNumber].x > segments[segNumber - 1].x) {
        segments[segNumber].dir = LEFT;
      }
      break;
    }
  }
}

// Check and react to collisions between numbers and the head of the snake
void checkSumCollisions() {
  for(int i = 0; i < MAX_SUMS; ++i) {
    if(segments[0].x + SNAKE_WIDTH >= sums[i].x && segments[0].x < sums[i].x + 5 &&
       segments[0].y + SNAKE_WIDTH >= sums[i].y && segments[0].y < sums[i].y + 7) {
      if(sums[i].num == expression.oppA + expression.oppB) {
        for(int i = 0; i < 3; ++i) {
          addSegment();
        }
      } else {
        removeSegment();
      }
      resetExpr = true;
      break;
    }
  }
}

// Move the entire snake
void moveSegments() {
  switch(segments[0].dir) {
  case RIGHT:
    segments[0].x += SNAKE_WIDTH;
    if(segments[0].x + SNAKE_WIDTH > LCD_RIGHT || checkSegmentCollisions()) {
      endGame = true;
    }
    break;
  case DOWN:
    segments[0].y += SNAKE_WIDTH;
    if(segments[0].y + SNAKE_WIDTH > LCD_BOTTOM || checkSegmentCollisions()) {
      endGame = true;
    }
    break;
  case LEFT:
    segments[0].x -= SNAKE_WIDTH;
    if(segments[0].x < LCD_LEFT || checkSegmentCollisions()) {
      endGame = true;
    }
    break;
  case UP:
    segments[0].y -= SNAKE_WIDTH;
    if(segments[0].y < LCD_TOP || checkSegmentCollisions()) {
      endGame = true;
    }
    break;
  }

  checkSumCollisions();

  for(int i = 1; i < snakeLength; ++i) {
    moveFollowingSegment(i);
  }
}

// Draw the boundary lines
void drawBounds() {
  rect(0, 0, LCD_RIGHT + 1, LCD_BOTTOM, 1);
  rect(1, 1, LCD_RIGHT - 1, LCD_BOTTOM - 2, 0);
  rect(LCD_RIGHT, 0, 10, LCD_BOTTOM, 1);
  rect(LCD_RIGHT + 1, 1, 8, LCD_BOTTOM - 2, 0);
}

// Draw the snake
void drawSnake() {
  for(int i = 0; i < snakeLength; ++i) {
    rect(segments[i].x, segments[i].y, SNAKE_WIDTH, SNAKE_WIDTH, 1);
  }
}

// Draw the math problem
void drawExpression() {
  setChar(expression.oppA, LCD_WIDTH - 8, 5, 1);
  setChar(9, LCD_WIDTH - 8, 15, 1);
  setChar(expression.oppB, LCD_WIDTH - 8, 25, 1);
}

// Draw the random numbers
void drawSums() {
  for(int i = 0; i < MAX_SUMS; ++i) {
    setChar(sums[i].num, sums[i].x, sums[i].y, 1);
  }
}

// Add a segment to the end of the snake
void addSegment() {
  if(snakeLength < MAX_SNAKE_LENGTH) {
    switch(segments[snakeLength - 1].dir) {
    case UP:
      segments[snakeLength].x = segments[snakeLength - 1].x;
      segments[snakeLength].y = segments[snakeLength - 1].y + SNAKE_WIDTH;
      break;
    case RIGHT:
      segments[snakeLength].x = segments[snakeLength - 1].x - SNAKE_WIDTH;
      segments[snakeLength].y = segments[snakeLength - 1].y;
      break;
    case LEFT:
      segments[snakeLength].x = segments[snakeLength - 1].x + SNAKE_WIDTH;
      segments[snakeLength].y = segments[snakeLength - 1].y;
      break;
    case DOWN:
      segments[snakeLength].x = segments[snakeLength - 1].x;
      segments[snakeLength].y = segments[snakeLength - 1].y - SNAKE_WIDTH;
      break;
    }
    segments[snakeLength].dir = segments[snakeLength - 1].dir;
    ++snakeLength;
  }
}

// Remove a segment from the end of the snake
void removeSegment() {
  if(snakeLength > 1) {
    segments[snakeLength - 1].x = -1;
    segments[snakeLength - 1].y = -1;
    segments[snakeLength - 1].dir = -1;
    --snakeLength;
  } else {
    endGame = true;
  }
}

// Game loop
void loop() {
  // put your main code here, to run repeatedly:
  
  if(digitalRead(RIGHT)) {
    if(segments[0].dir != LEFT) {
      segments[0].dir = RIGHT;
    }
  } else if(digitalRead(DOWN)) {
    if(segments[0].dir != UP) {
      segments[0].dir = DOWN;
    }
  } else if(digitalRead(LEFT)) {
    if(segments[0].dir != RIGHT) {
      segments[0].dir = LEFT;
    }
  } else if(digitalRead(UP)) {
    if(segments[0].dir != DOWN) {
      segments[0].dir = UP;
    }
  }

  moveSegments();

  if(resetExpr) {
    resetExpr = false;
    RESET_EXPRESSION;
  }

  if(endGame) {
    resetExpr = false;
    endGame = false;
    RESET_SNAKE;
    RESET_EXPRESSION;
  }

  drawBounds();

  /* for(int i = 0; i < 10; ++i) {
    setChar(i, 13 + 5 * i, 9, 1);
  } */

  drawExpression();
  drawSums();
  
  drawSnake();

  updateDisplay();
  
  delay(200);
}
