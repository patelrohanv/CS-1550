/*
 * Compile: gcc -o graphics graphics.c
 * Execute: ./graphics
 */

#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "iso_font.h"
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/mman.h>


typedef unsigned short color_t;    // |15 11|10  5|4  0|
                                   // |red  |green|blue|
                                   //   5   + 6   + 5  =  16
#define BMASK(c) (c & 0x001F) // Blue mask
#define GMASK(c) (c & 0x07E0) // Green mask
#define RMASK(c) (c & 0xF800) // Red mask



int fid; //file ID
size_t size;
size_t map_size;
//unsigned long size;
unsigned short *address;       // Pointer to the shared memory space with fb
struct termios keypress;
unsigned long x_len;
unsigned long y_len;
unsigned long line_length;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_line(int x1, int y1, int x2, int y2, color_t c);
int absolute_val (int x);
void draw_text(int x, int y, const char *text, color_t c);
void draw_char(int x, int y, const char *text, color_t c);




void init_graphics(){
//open, ioctl, mmap
    // 1. Open fb file descriptor
    fid = open("/dev/fb0", O_RDWR);
    if(fid == -1)
    {
        exit(1);
    }
    //2. memory mapping
    struct fb_var_screeninfo fb_var; //virtual resolution
    ioctl(fid, FBIOGET_VSCREENINFO, &fb_var);
    struct fb_fix_screeninfo fb_fix; //bit depth
    ioctl(fid, FBIOGET_FSCREENINFO, &fb_fix);
    x_len =  fb_var.xres_virtual;
    y_len = fb_var.yres_virtual;


    map_size = fb_var.yres_virtual * fb_fix.line_length;
    //y resolution * length of a line in bytes 
    // Add a memory mapping
    address = (unsigned short *) mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fid, 0);
    
    //4. disable keypress echo
    ioctl(STDIN_FILENO, TCGETS, &keypress);
    keypress.c_lflag &= ~ICANON;
    keypress.c_lflag &= ~ECHO;
    ioctl(1, TCSETS, &keypress);
}

void exit_graphics(){
//itoctl, close, munmap
    // Remove the memory mapping
    if(munmap(address, map_size) == -1)
    {
        exit(1);
    }

    //enable keypress echo
    ioctl( 1, TCGETS, &keypress ); 
    keypress.c_lflag |= ECHO;
    keypress.c_lflag |= ICANON;
    ioctl( 1, TCSETS, &keypress );

    // Close fb file descriptor
    if(!close(fid))
    {
        exit(0);
    }
    else
    {
        exit(1);
    }
    

}

void clear_screen(){
//write
    //const char clear_msg[] = "\033[2J";
    write(1, "\033[2J", 8);
}

char getkey(){
//select, read
//int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
//ssize_t read(int fd, void *buf, size_t count);
	//int nfds;
    fd_set readfds;
    //fd_set writefds;
    //fd_set exceptfds;
    struct timeval timeout;
    char ret;
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if (select(1, &readfds, NULL, NULL, &timeout)){
        read(0, &ret, sizeof(char));
        return ret;
    }
    else{
        return '\0';
    }
}

void sleep_ms(long ms){
//nanosleep
	struct timespec sleep_spec;

    sleep_spec.tv_sec = 0;
    sleep_spec.tv_nsec = ms*1000000;

	nanosleep(&sleep_spec ,NULL);
}

void draw_pixel(int x, int y, color_t color){
    if(x >= 0 && y >= 0 && x < x_len && y < y_len){
        unsigned long x_coordinate = x;
        unsigned long y_coordinate = (line_length/2) * y;
        // unsigned short *p = address + x_coordinate + y_coordinate;
        address[y*640+x] = color;

        // *p = color;
    }
    else{
        return;
    }
}

void draw_line(int x1, int y1, int x2, int y2, color_t c){
    int dx = absolute_val((x2-x1)), sx = x1<x2 ? 1 : -1;
    int dy = absolute_val((y2-y1)), sy = y1<y2 ? 1 : -1; 
    int err = (dx>dy ? dx : -dy)/2, e2;
 
    for(;;){
        draw_pixel(x1,y1,c);
        if (x1==x2 && y1==y2) break;
        e2 = err;
        if (e2 >-dx) { err -= dy; x1 += sx; }
        if (e2 < dy) { err += dx; y1 += sy; }
    }

}

int absolute_val (int x){
    if (x < 0){
        return (x*(-1));
    }
    return x;
}

void draw_text(int x, int y, const char *text, color_t c){
    int i = (int) text;
    int j = iso_font[i];

}
void draw_char(int x, int y, const char *text, color_t c){
    
}