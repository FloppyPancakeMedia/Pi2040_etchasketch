#include <string>

const char options[2][32] = {"Clear", "Save"};
const int16_t b_width = 50, b_height = 20;


struct Button{
  int16_t x, y, width = 50, height = 20;
  uint16_t normalColor;
  uint16_t highlightColor;
  char action[32];
};

Button buttons[2];

void setupMenu(){
  for (int i = 0; i < 2; i++){
    Button b = Button();
    strncpy(b.action, options[i], 32);
    b.x = DIMENSIONS - b.width;
    if (i == 0){
      b.y = DIMENSIONS - b.height;
    }
    else{
      b.y = buttons[i-1].y - b.height;
    }
    b.normalColor = colors[6]; // Orange
    b.highlightColor = colors[0]; 
  }
}

void showMenu(){
  for (int i = 0; i < 2; i++){
    tft.drawRect(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height, buttons[i].normalColor);
    tft.setCursor(buttons[i].x + 10, buttons[i].y + 10);
    tft.print(buttons[i].action);
  }
}

