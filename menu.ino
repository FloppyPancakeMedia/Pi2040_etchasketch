#include <SD.h>
#include <SPI.h>

#define SAVE "Save"
#define CLEAR "Clear"
#define EXIT "Exit"
#define NUM_BUTTONS 3

struct Button{
  int16_t x, y;
  char action[32];

  Button(int16_t new_x, int16_t new_y, char *a){
    x = new_x;
    y = new_y;
    strcpy(action, a);
  }

  Button(){
    x = 0;
    y = 0;
    action[0] = '\0';
  }
};

struct Menu {
  int16_t buttonWidth = 90, buttonHeight = 40;
  int16_t Height = 0, Width = 90;
  Button buttons[3];
  uint16_t normalColor;
  uint16_t highlightColor;
};

Menu menu;
Button *curButton = nullptr, *prevButton = nullptr;
uint8_t curButtonIdx = 0;

void setupMenu(){
  // TODO: LEFT OFF WITH BOARD CRASHING BEFORE PROGRAM CAN RUN
  if (!SPI1.setCS(SD_CS)) Serial.println("Failed to set CS");
  if (!SPI1.setMISO(SD_MISO)) Serial.println("Failed to set MISO");
  if (!SPI1.setMOSI(SD_MOSI)) Serial.println("Failed to set MOSI");
  if (!SPI1.setSCK(SD_CLK)) Serial.println("Failed to set CLK");

  if (!SD.begin(SD_CS)){
    Serial.println("Failed to init SD card reader...");
  }
  else{
    Serial.println("Writing new file");
    File file = SD.open("test.txt", FILE_WRITE);
    file.println("Testing testing one tooo");
    file.close();
    Serial.println("Done writing file");
    Serial.println();

    Serial.println("Attempting to read file");
    file = SD.open("test.txt");
    if (file){
      while (file.available()) Serial.write(file.read());
      file.close();
    }
    Serial.println();
    Serial.println("Done reading file");
  }

  menu.highlightColor = colors[0];
  menu.normalColor = colors[6];

  for (int i = 0; i < NUM_BUTTONS; i++){
    int16_t x = DIMENSIONS - menu.buttonWidth;
    int16_t y = 0;
    if (i == 0) y = DIMENSIONS - menu.buttonHeight;
    else y = menu.buttons[i-1].y - menu.buttonHeight;

    char action[32];
    if (i == 0) strcpy(action, SAVE);
    else if (i == 1) strcpy(action, CLEAR);
    else if (i == 2) strcpy(action, EXIT);
    Button b = Button(x, y, action);

    menu.buttons[i] = b;
  }

  prevButton = curButton = &menu.buttons[0];

  menu.Height = NUM_BUTTONS * menu.buttonHeight;
}

void runMenu(){
  if (curState == MENU_OPENING){
    showMenu();
  }
  else if (curState == CHOOSING_OPTION){
    uint8_t direction = getRotaryR();
    
    if (direction != 0) {
      // Save prev button for redrawing
      prevButton = &menu.buttons[curButtonIdx];
      
      // Up
      if (direction == 1) {
        curButtonIdx++;
        if (curButtonIdx >= NUM_BUTTONS) curButtonIdx = 0; 
      }
      // Down
      else {
        if (curButtonIdx == 0) curButtonIdx = NUM_BUTTONS - 1;
        else curButtonIdx--;
      }
      
      curButton = &menu.buttons[curButtonIdx];
      highlightButton();
    }
  }
    
  else if (curState == OPTION_SELECTED){
    char *action = curButton->action;
    
    if (strcmp(action, CLEAR) == 0){
      clearScreen();
      curState = OPTION_COMPLETE;
    }
    else if (strcmp(action, SAVE) == 0){

    }
    else if (strcmp(action, EXIT) == 0){
      hideMenu();
      curState = OPTION_COMPLETE;
    }
  }
}

void highlightButton(){
  tft.drawRect(prevButton->x, prevButton->y, menu.buttonWidth + 1, menu.buttonHeight, menu.normalColor);
  tft.drawRect(curButton->x, curButton->y, menu.buttonWidth + 1, menu.buttonHeight, menu.highlightColor);
}

void showMenu(){
  tft.setTextSize(2);
  for (int i = 0; i < NUM_BUTTONS; i++){
    tft.drawRect(menu.buttons[i].x, menu.buttons[i].y, menu.buttonWidth, menu.buttonHeight, menu.normalColor);
    tft.fillRect(menu.buttons[i].x + 1, menu.buttons[i].y + 1, menu.buttonWidth - 1, menu.buttonHeight -1, ST77XX_BLACK);
    tft.setCursor(menu.buttons[i].x + 10, menu.buttons[i].y + 10);
    tft.print(menu.buttons[i].action);
  }
  curState = CHOOSING_OPTION;

  // Reset default button
  curButton = &menu.buttons[0];
  prevButton = &menu.buttons[0];
  curButtonIdx = 0;

  highlightButton();
}

void hideMenu(){
  repaintFromBitmap(menu.buttons[NUM_BUTTONS - 1].x, menu.buttons[NUM_BUTTONS - 1].y, menu.Width, menu.Height, true);
}

