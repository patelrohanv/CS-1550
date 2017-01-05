#include "library.c"

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_line(int x1, int y1, int x2, int y2, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

int main (){
  char c = '\0';

  init_graphics();
  clear_screen();
  do{
    draw_pixel(10, 30, 0x001F);
    draw_pixel(53, 100, 0x07E0);

    draw_line(1,300,50, 20, 0xF800);
    draw_line(5,5,100, 150, 0x001F);
    draw_line(2,5,63, 89, 0x07E0);
    draw_line(1,1,630, 470, 0xF800);
    draw_line(100, 100,50, 50, 0x2347);



     c = getkey();  

  }while(c != 'q');
  clear_screen();
  exit_graphics();
  return 0;
}