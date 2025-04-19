#include "DEV_Config.h"
#include "LCD_1in3.h"
#include "GUI_Paint.h"
#include "GUI_BMP.h"
#include "init.h"
#include "utils.h"
#include "Debug.h"
#include <stdio.h>	//printf()
#include <stdlib.h> //exit()
#include <signal.h> //signal()
#include <stdbool.h>
#include <sys/resource.h>
#include <systemd/sd-journal.h>
#include <pthread.h>
#include "i2c_utils.h"

// Required for getting version from CMAKE
#define Q(x) #x
#define QUOTE(x) Q(x)

extern const char *__progname;
pthread_t thread;
bool display_toggle = true;
int display_dim[] = {0, 64 - 1, 128 - 1, 256 - 1, 512 - 1, 1024 - 1};
int display_dim_idx = (sizeof(display_dim) / sizeof(display_dim[0])) - 1;
int display_dim_max = (sizeof(display_dim) / sizeof(display_dim[0])) - 1;

#define CHECK_PROCESS_STATUS "firefox"

#define LINE_HEIGHT_ADJUSTMENT 5
#define LINE_WIDTH_ADJUSTMENT 12

void HANDLER_set_LCD_brigtness(int signo);

void LCD_1IN3_init(void)
{
	char ip_address[20] = "";  // 16
	char time_buffer[30] = ""; // 26
	char uptime[20] = "";
	char load[20] = "";
	char cpu_temp[20] = "";
	int battery_percentage = 0;
	char battery[20] = "";
	UWORD batt_colour = BLACK;
	int netstatus = 0;
	char process_status[1024] = "";
	clock_t tic = clock();
	clock_t toc = clock();
	struct timeval start_s;
	struct timeval start_u;
	struct timeval end_s;
	struct timeval end_u;
	bool on_battery;
	int time_remaining_or_to_full = 0;
	int time_ups_hours = 0;
	int time_ups_miniutes = 0;
	char ups_time[20] = "";

	int who = RUSAGE_SELF;
	struct rusage usage;
	int ret;

	if (!(ret = getrusage(who, &usage)))
	{
		start_s = usage.ru_stime;
		start_u = usage.ru_utime;
	}

	signal(SIGINT, HANDLER_exit);
	signal(SIGHUP, HANDLER_exit);
	signal(SIGKILL, HANDLER_exit);
	signal(SIGQUIT, HANDLER_exit);
	signal(SIGTERM, HANDLER_exit);
	signal(SIGUSR1, HANDLER_set_LCD_brigtness);
	signal(SIGUSR2, HANDLER_set_LCD_brigtness);

	/* Module Init */
	if (DEV_ModuleInit() != 0)
	{
		DEV_ModuleExit();
		exit(0);
	}

	/* LCD Init */
	LCD_1IN3_Init(HORIZONTAL);
	LCD_1IN3_Clear(BLACK);
	DEBUG("DISPLAY DIM: %d %d %d\n", sizeof(display_dim), sizeof(display_dim[0]), display_dim[display_dim_max]);
	LCD_SetBacklight(display_dim[display_dim_max]);

	UWORD *BlackImage;
	UDOUBLE Imagesize = LCD_1IN3_HEIGHT * LCD_1IN3_WIDTH * 2;
	DEBUG("Imagesize = %d", Imagesize);
	if ((BlackImage = (UWORD *)malloc(Imagesize)) == NULL)
	{
		sd_journal_perror("Failed to apply for black memory...");
		exit(0);
	}
	// /*1.Create a new image cache named IMAGE_RGB and fill it with white*/
	Paint_NewImage(BlackImage, LCD_1IN3_WIDTH, LCD_1IN3_HEIGHT, 0, BLACK, 16);
	Paint_Clear(BLACK);
	Paint_SetRotate(ROTATE_0);

	strcpy(process_status, "Process [");
	strcat(process_status, CHECK_PROCESS_STATUS);
	strcat(process_status, "]: %d");

	sd_journal_print(LOG_INFO, "[%s v%s] started", __progname, QUOTE(PROJECT_VERSION));

	if (pthread_create(&thread, NULL, check_keys, NULL))
	{
		sd_journal_perror("Error starting  buttons polling thread, buttons won't work");
	}
	else
	{
		sd_journal_print(LOG_INFO, "Started buttons polling thread");
	}

	while (1)
	{
		getCurrentDateTime(time_buffer);
		getIPAddr(ip_address, "wlan0");
		get_uptime(uptime);
		GetCPULoad(load);
		get_CPU_temp(cpu_temp);
		netstatus = net_status();
		battery_percentage = i2c_read_reg(I2C_ADDR_BATTERY_PERCENT);
		snprintf(battery, 20 - 1, "%d %%", battery_percentage);
		on_battery = !(i2c_read_reg(I2C_ADDR_CHARGE_STATUS) & I2C_MASK_VBUS_POWERED); // should be '1' if on battery

		if (battery_percentage > 50)
			batt_colour = GREEN;
		else if (battery_percentage > 20)
			batt_colour = YELLOW;
		else
			batt_colour = RED;

		if (on_battery)
			time_remaining_or_to_full = i2c_read_reg(I2C_ADDR_BATTERY_REMAINING_DISCHARGE_TIME);
		else
			time_remaining_or_to_full = i2c_read_reg(I2C_ADDR_BATTERY_REMAINING_CHARGE_TIME);

		if (time_remaining_or_to_full >= 0xBB80) // 800 hours
		{
			time_remaining_or_to_full = 0;
		}

		time_ups_hours = (int)time_remaining_or_to_full / 60;
		time_ups_miniutes = (int)time_remaining_or_to_full % 60;

		snprintf(ups_time, 20 - 1, time_ups_hours ? "%d h" : "%d min", time_ups_hours ? time_remaining_or_to_full / 60 : time_remaining_or_to_full % 60);

		DEBUG("It's now %s", time_buffer);
		DEBUG("IP: %s", ip_address);
		DEBUG("Uptime: %s", uptime);
		DEBUG("CPU load: %s", load);
		DEBUG("CPU temp: %s", cpu_temp);
		DEBUG("Internet: %d", netstatus);
		DEBUG(process_status, is_process_running(CHECK_PROCESS_STATUS));
		DEBUG("Battery %: %s", battery);

		Paint_Clear(BLACK);
		Paint_DrawRectangle(2, 2, LCD_1IN3_WIDTH - 2, 28, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
		Paint_DrawString_EN(7, 7, time_buffer, &Font20, WHITE, BLACK);
		//
		Paint_DrawString_EN(5, 37, "IP Address", &Font12, BLACK, MAGENTA);
		Paint_DrawString_EN(LCD_1IN3_WIDTH - strlen(ip_address) * Font20.Width - LINE_WIDTH_ADJUSTMENT, 57, ip_address, &Font20, BLACK, WHITE);
		Paint_DrawLine(5, 57 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, LCD_1IN3_WIDTH - 5, 57 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
		//
		Paint_DrawString_EN(5, 87, "Uptime", &Font12, BLACK, MAGENTA);
		Paint_DrawString_EN(LCD_1IN3_WIDTH - strlen(uptime) * Font20.Width - LINE_WIDTH_ADJUSTMENT, 107, uptime, &Font20, BLACK, WHITE);
		Paint_DrawLine(5, 107 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, LCD_1IN3_WIDTH - 5, 107 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
		//
		Paint_DrawString_EN(5, 137, "CPU Load", &Font12, BLACK, MAGENTA);
		Paint_DrawString_EN(LCD_1IN3_WIDTH / 2 - strlen(load) * Font20.Width - LINE_WIDTH_ADJUSTMENT, 157, load, &Font20, BLACK, WHITE);
		//
		Paint_DrawString_EN(5 + LCD_1IN3_WIDTH / 2, 137, on_battery ? "On Battery" : "Battery", &Font12, BLACK, MAGENTA);
		Paint_DrawString_EN(LCD_1IN3_WIDTH - strlen(battery) * Font20.Width - LINE_WIDTH_ADJUSTMENT, 157, battery, &Font20, BLACK, batt_colour);
		Paint_DrawLine(5, 157 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, LCD_1IN3_WIDTH / 2, 157 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
		Paint_DrawLine(LCD_1IN3_WIDTH / 2, 157 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, LCD_1IN3_WIDTH - 5, 157 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
		//
		Paint_DrawString_EN(5, 187, "CPU Temperature", &Font12, BLACK, MAGENTA);
		Paint_DrawString_EN(LCD_1IN3_WIDTH / 2 - strlen(cpu_temp) * Font20.Width - LINE_WIDTH_ADJUSTMENT, 207, cpu_temp, &Font20, BLACK, WHITE);
		//
		Paint_DrawString_EN(5 + LCD_1IN3_WIDTH / 2, 187, on_battery ? "Time To Empty" : "Time To Full", &Font12, BLACK, MAGENTA);
		Paint_DrawString_EN(LCD_1IN3_WIDTH - strlen(ups_time) * Font20.Width - LINE_WIDTH_ADJUSTMENT, 207, ups_time, &Font20, BLACK, on_battery & (time_remaining_or_to_full < 20) ? RED : WHITE);
		Paint_DrawLine(5, 207 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, LCD_1IN3_WIDTH - 5, 207 + Font20.Height + LINE_HEIGHT_ADJUSTMENT, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
		//
		Paint_DrawLine(LCD_1IN3_WIDTH / 2, 132, LCD_1IN3_WIDTH / 2, 240 - 8, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

		if (netstatus)
		{
			Paint_DrawChar(10, 57, 'X', &Font20, RED, BLACK);
		}
		else
		{
			Paint_DrawChar(10, 57, '~', &Font20, GREEN, BLACK);
		}
		DEBUG("drawing...");

		LCD_1IN3_Display(BlackImage);

		// HERE

		toc = clock();
		DEBUG("%f seconds elaspsed.\n", (double)((toc - tic) / CLOCKS_PER_SEC));

		if (!(ret = getrusage(who, &usage)))
		{
			end_s = usage.ru_stime;
			end_u = usage.ru_utime;
		}

		DEBUG(LOG_INFO, "%s getrusage() gives %d seconds.\n", __progname, end_s.tv_sec + end_u.tv_sec - start_s.tv_sec - start_u.tv_sec);

		sleep(10);
	}

	if (pthread_cancel(thread))
	{
		sd_journal_perror("Error stopping buttons polling thread");
	}
	else
	{
		sd_journal_print(LOG_INFO, "Stopped buttons polling thread");
	}

	LCD_1IN3_Clear(BLACK);
	free(BlackImage);
	BlackImage = NULL;
	DEV_ModuleExit();
}

void HANDLER_set_LCD_brigtness(int signo)
{
	if (signo == SIGUSR1) // dim display
	{
		display_dim_idx = 0;
		display_toggle = false;
	}
	else if (signo == SIGUSR2) // crank-up display
	{
		display_dim_idx = display_dim_max;
		display_toggle = true;
	}

	LCD_SetBacklight(display_dim[display_dim_idx]);

	return;
}
