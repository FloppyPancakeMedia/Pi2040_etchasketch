#include <Adafruit_ST7789.h>
#include <map>

/* 
  ##############################
  ######## HARDWARE VARS #######
  ##############################
*/
#define RGB888to565(r, g, b) (uint16_t)( ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3) )
#define DIMENSIONS 240

#define PICO1

// #ifdef RP2040MINI
//   const int D_SDA = 0, D_SCL = 2, D_RST = 1, D_DC = 4, D_CS = 5;
//   const int SD_SDA = 8, SD_SCL = 10, SD_CS = 9, SD_MISO = 11;
//   const int SWR = 15, DTR = 27, CLKR = 28;
//   const int SWL = 7, DTL = 13, CLKL = 14;
//   const int LED_DEFAULT = 16;
//   const int FADER = 29;
// #endif

#ifdef PICO1
  const int D_SDA = 19, D_SCL = 18, D_RST = 21, D_DC = 20, D_CS = 17;
  const int SD_CS = 13, SD_MOSI = 12, SD_CLK = 14, SD_MISO = 15;
  const int DTL = 3, CLKL = 2, SWL = 4;
  const int DTR = 7, CLKR = 6, SWR = 8;
  const int FADER = 26;
  const int LED_BOARD = 25;
#endif

Adafruit_ST7789 tft = Adafruit_ST7789(D_CS, D_DC, D_SDA, D_SCL, D_RST);

/* 
  ###################################
  ######## END HARDWARE VARS ########
  ###################################
*/

std::map<uint8_t, uint16_t> colors = {
  {0, ST77XX_BLUE},
  {1, ST77XX_CYAN},
  {2, ST77XX_GREEN},
  {3, 0x897b}, // BlueViolet
  {4, ST77XX_MAGENTA},
  {5, ST77XX_YELLOW},
  {6, ST77XX_ORANGE},
  {7, ST77XX_RED},
};

std::map<uint16_t, const char*> colorToText = {
  {ST77XX_BLUE, "Blue"},
  {ST77XX_CYAN, "Cyan"},
  {ST77XX_GREEN, "Green"},
  {0x897b, "Blue Violet"},
  {ST77XX_MAGENTA, "Magenta"},
  {ST77XX_YELLOW, "Yellow"},
  {ST77XX_ORANGE, "Orange"},
  {ST77XX_RED, "Red"}
};

uint16_t bitmap[DIMENSIONS][DIMENSIONS];

void setup() {
  Serial.begin(57600);
  delay(100);
  Serial.println("I'm here...");

  tft.init(DIMENSIONS, DIMENSIONS);  
  tft.fillScreen(ST77XX_BLACK); 
  tft.setTextSize(1);

  pinMode(SWR, INPUT_PULLUP);
  pinMode(DTR, INPUT_PULLUP);
  pinMode(CLKR, INPUT_PULLUP);

  pinMode(CLKL, INPUT_PULLUP);
  pinMode(DTL, INPUT_PULLUP);
  pinMode(SWL, INPUT_PULLUP);

  pinMode(LED_BOARD, OUTPUT);
  pinMode(FADER, INPUT);

  digitalWrite(LED_BOARD, HIGH);
  delay(1000);
  digitalWrite(LED_BOARD, LOW);

  attachInterrupt(digitalPinToInterrupt(SWL), toggleDrawing, FALLING);
  attachInterrupt(digitalPinToInterrupt(SWR), swrPressed, FALLING);

  // Init bitmap to all black
  for (int i = 0; i < DIMENSIONS; i++){
    for (int j = 0; j < DIMENSIONS; j++){
      bitmap[i][j] = ST77XX_BLACK;
    }
  }

  setupMenu();
}

/* 
  ###################################
  ####### PROG SPECIFIC VARS ########
  ###################################
*/

/******** STATES *********/
#define DRAWING 0
#define FLOATING 1
#define MENU_OPENING 2
#define MENU_CLOSING 3
#define CHOOSING_OPTION 4
#define OPTION_SELECTED 5
#define OPTION_COMPLETE 6
/*************************/
uint8_t curState = DRAWING;
uint8_t savedState = DRAWING; // Saves either drawing or floating

uint8_t faderCheckTimer = 0;
uint32_t textTimer = 0;
const uint8_t TIMER_MAX = 10;
uint8_t textTimerOn = 0;

bool switchedToDrawing = false;

int16_t prevFaderRead = 0;
uint8_t prevClkR = 0, prevClkL = 0;
uint8_t prevDTR = 0;

uint16_t curX = 120, curY = 120;
uint16_t prevX = 120, prevY = 120;

const uint16_t colorSelWidth = DIMENSIONS / 4, colorSelHeight = 10, colorSelX = (DIMENSIONS / 4) + (colorSelWidth / 2), colorSelY = 5;

uint16_t foregroundColor = ST77XX_MAGENTA;
uint16_t prevColor;

void hideMenu();
void runMenu();

/* 
  #####################################
  ########## END DECLARATIONS #########
  #####################################
*/


void loop() {
  // Read fader to check if color change needed
  if (curState == DRAWING || curState == FLOATING){
    if (switchedToDrawing){
      tft.drawPixel(curX, curY, bitmap[curX][curY]); // Rewrite white pixel to prev color
      switchedToDrawing = false;
    }

    checkFader();

    // Worry about if text is on screen
    if (textTimerOn == 1){
      textTimer++;
      if (textTimer >= 1000000){
        clearText();
      }
    }
    
    // Main drawing loop
    runDraw();
  }
  else if (curState == OPTION_COMPLETE){
    curState = savedState;
  }
  else{
    runMenu();
  }
}

void checkFader(){
  faderCheckTimer += 1;
  if (faderCheckTimer >= TIMER_MAX){
    int16_t faderRead = analogRead(FADER);
    faderRead = constrain(faderRead, 0, 950);
    faderRead = map(faderRead, 5, 950, 0, 7);

    if (faderRead != prevFaderRead){
      changeColor(faderRead);
    }
    prevFaderRead = faderRead;
    faderCheckTimer = 0;
  }
}

void changeColor(int16_t faderRead){
  // Clear previous, if exists
  repaintFromBitmap(colorSelX, colorSelY, colorSelWidth, colorSelHeight, false);

  // Change color and display message
  foregroundColor = colors[faderRead];

  tft.fillRect(colorSelX, colorSelY, colorSelWidth, colorSelHeight, foregroundColor);
  textTimerOn = 1;
}

/* DISCLAIMER: This function was produced by Claude AI. I gave it a shot. It did better*/
void repaintFromBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fancy){
  if (fancy){
    uint16_t total = w * h;
    uint16_t *indices = (uint16_t*)malloc(total * sizeof(uint16_t));
    if (!indices) return; // allocation failed

    for (uint16_t i = 0; i < total; i++) indices[i] = i;

    // Fisher-Yates shuffle
    for (uint16_t i = total - 1; i > 0; i--) {
        uint16_t j = rand() % (i + 1);
        uint16_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    for (uint16_t i = 0; i < total; i++) {
        uint16_t px = x + (indices[i] % w);
        uint16_t py = y + (indices[i] / w);
        tft.drawPixel(px, py, bitmap[px][py]);
    }

    free(indices);
  }
  /* Aside from this part. I wrote this.*/
  else{
    for (uint16_t i = x; i < x + w; i++){
      for (uint16_t j = y; j < y + h; j++){
        tft.drawPixel(i, j, bitmap[i][j]);
      }
    }
  }
}
/* End AI cheat section */

void runDraw(){  
  prevX = curX;
  prevY = curY;

  // Check for X movement
  int clkRead = digitalRead(CLKR);
  if (prevClkR != clkRead && clkRead == LOW){
    Serial.printf("clkRead: %d | prevClkR: %d | Dtr: %d\n", clkRead, prevClkR, digitalRead(DTR));
    if (digitalRead(DTR) != clkRead){
      curX++;
    }
    else{
      curX--;
    }
  }

  // Check for Y movement
  int clkReadL = digitalRead(CLKL);
  if (clkReadL != prevClkL && clkReadL == LOW){
    Serial.println("Drawing Y");
    if (digitalRead(DTL) != clkReadL){
      curY++;
    }
    else{
      curY--;
    }
  }

  // Check X and Y bounds and clamp
  if (curX >= DIMENSIONS - 2) curX = DIMENSIONS - 2;
  else if (curX <= 2) curX = 2;

  if (curY >= DIMENSIONS - 2) curY = DIMENSIONS - 2;
  else if (curY <= 11) curY = 11;

  // Draw
  if (prevX != curX || prevY != curY){
    if(curState == FLOATING){
      tft.drawPixel(prevX, prevY, bitmap[prevX][prevY]);
      tft.drawPixel(curX, curY, ST77XX_WHITE);
    }
    else{
      tft.drawPixel(curX, curY, foregroundColor);
      bitmap[curX][curY] = foregroundColor;
    }
  }

  prevClkR = clkRead;
  prevClkL = clkReadL;
}



void clearText(){ 
  repaintFromBitmap(colorSelX, colorSelY, colorSelWidth, colorSelHeight, true);
  // Reset timer
  textTimerOn = 0;
  textTimer = 0;
}

void clearScreen(){
  Serial.println("Clearing screen");
  tft.fillScreen(ST77XX_BLACK);
  // Reset bitmap
  for (int i = 0; i < DIMENSIONS; i++){
    for (int j = 0; j < DIMENSIONS; j++){
      bitmap[i][j] = ST77XX_BLACK;
    }
  }
  curX = 120;
  curY = 120;
}

/*
############################
#### Interrupt Routines ####
############################
*/

void toggleDrawing(){
  // Debounce
  static uint32_t lastPress = 0;
  uint32_t now = millis();
  if (now - lastPress < 200) return; // ignore if <200ms since last press
  lastPress = now;
  
  if (curState == FLOATING){
    switchedToDrawing = true;
    curState = DRAWING;
  } 
  else if (curState == DRAWING){
    curState = FLOATING;
  }
  else;
  
}

void swrPressed(){
  // Debounce
  static uint32_t lastPress = 0;
  uint32_t now = millis();
  if (now - lastPress < 200) return; // ignore if <200ms since last press
  lastPress = now;

  if (curState == DRAWING || curState == FLOATING){
    savedState = curState;
    curState = MENU_OPENING;
  }
  else if (curState == CHOOSING_OPTION){
    curState = OPTION_SELECTED;
  }
}

