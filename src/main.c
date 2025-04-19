#include "init.h"
#include <math.h>
#include <stdlib.h> //exit()
#include <stdio.h>
#include "../lib/LCD/LCD_1in3.h"
#include "../lib/Fonts/fonts.h"
#include "../lib/GUI/GUI_Paint.h"

extern const char *__progname;

int main(int argc, char *argv[])
{
	LCD_1IN3_init();

	return 0;
}
