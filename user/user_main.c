//Copyright 2015 <>< Charles Lohr, see LICENSE file.
//Copyright 2019 <>< Aaron Angert, see LICENSE file.
//Work in Progress

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "uart.h"
#include "osapi.h"
#include "espconn.h"
#include "esp82xxutil.h"
#include "ws2812.h"
#include "commonservices.h"
#include "vars.h"
#include "gpio.h"
#include "hash.c"
#include <mdns.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "gpio_buttons.h"

#define procTaskPrio        0
#define procTaskInit        1
#define procTaskopts        2

#define procTaskQueueLen    3



#define HUMAN 0
#define ZOMBIE 1
#define SUPERZOMBIE 2
#define DEAD 3

#define REDTEAM 4
#define GREENTEAM 5
#define BLUETEAM 6

#define NOTEAM 7


#define NOBUTTONS 208
#define ABUTTON 221
#define BBUTTON 210
#define BOTHBUTTONS 223

static volatile os_timer_t game_timer;
static volatile os_timer_t end_timer;
static volatile os_timer_t begin_timer;

static volatile os_timer_t game_timer;
static volatile os_timer_t end_timer;
static volatile os_timer_t begin_blob_timer;

static struct espconn *pUdpServer;
usr_conf_t * UsrCfg = (usr_conf_t*)(SETTINGS.UserData);
//uint8_t last_leds[40*3] = {0};
uint8_t leds[16*3] = {0};
// const int N = 300;
hashtable_t *hashtable;

//status of the beacon, and last three recorded rssi values
struct beacon_stat {
	char *type;
	int rssi[3];
};

int state;
int prev_state;

char states[][32] = {"Human",   "Zombie", "SuperZombie", "Dead", "RedTeam", "GreenTeam", "BlueTeam", "NoTeam" };

int normal_radar[7] = {-90, -85, -80, -70, -60, -50, -40};

int human_radar[7] = {-95, -90, -85, -80, -70, -60, -55};



int dead_amount_inc[7] = {40, 80, 120, 160, 200, 240, 280};

int blob_radar[7] = {348, 690, 1032, 1374, 1716, 2058, 2400};


int undead_amount_inc[8] = {0, 66, 128, 190, 252, 314, 376, 438};

int zombie_health_inc[8] = {0, 600, 1200, 1800, 2400, 3000, 3600, 4200};

int colors[12*3] = {0x01, 0x01, 0x01,
									0x00, 0x01, 0x00,
									0x00, 0x04, 0x00,
									0x01, 0x00, 0x00,
									0x01, 0x00, 0x00,
									0x00, 0x01, 0x00,
								  0x00, 0x00, 0x01,
								  0x01, 0x01, 0x01};

bool end_game = false;

bool button_pressed = false;
int dead_amount = 0;
int zombie_health = 4800;
int zombie_health_max = 4800;
int undead_amount = 0;
int undead_max = 460;


//9400
//4800
int power_depletion = 4800;


int light_level = 0x30;

bool show_time = false;

int timer_index = 2;
int times[8] = {5, 10, 15, 20, 30, 40, 50, 60};

int sensitivity_index = 5;
float sensitivities[7] = {-30.0, -32.0, -34.0, -36.0, -38.0, -40.0, -42.0};

int blob_health = 300;


void ICACHE_FLASH_ATTR user_scan(void);
void ICACHE_FLASH_ATTR user_scan_blob(void);


//int ICACHE_FLASH_ATTR StartMDNS();

void user_rf_pre_init(void) { /*nothing*/ }

char * strcat( char * dest, char * src )
{
    return strcat(dest, src );
}



//Tasks that happen all the time.
os_event_t    procTaskQueue[procTaskQueueLen];

//Timer event.
int count_down_time = 16;
static void ICACHE_FLASH_ATTR gameTimer(void *arg)
{
	if (count_down_time <= 1)
	{
		os_timer_disarm(&game_timer);
		end_game = true;
	}
	count_down_time -= 1;
}


int begin_time = 8;
static void ICACHE_FLASH_ATTR begin_game_func(void *arg)
{
	make_radar_full(leds, 1, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], begin_time);
	make_radar_full(leds, 0, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], begin_time);
	WS2812OutBuffer( leds, sizeof(leds), light_level );
	if (begin_time <= 0)
	{
		os_timer_disarm(&begin_timer);
		os_timer_disarm(&game_timer);
		os_timer_setfn(&game_timer, (os_timer_func_t *)gameTimer, NULL);
		os_timer_arm(&game_timer, (times[timer_index] * 60 *1000)/16, 1);
		system_os_task(user_scan, procTaskPrio, procTaskQueue, procTaskQueueLen);
		system_os_post(procTaskPrio, 0, 0 );
	}
	begin_time -= 1;
}


static void ICACHE_FLASH_ATTR begin_game_func_blob(void *arg)
{
	make_radar_full(leds, 1, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], begin_time);
	make_radar_full(leds, 0, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], begin_time);
	WS2812OutBuffer( leds, sizeof(leds), light_level );
	if (begin_time <= 0)
	{
		os_timer_disarm(&begin_timer);
		os_timer_disarm(&game_timer);
		os_timer_setfn(&game_timer, (os_timer_func_t *)gameTimer, NULL);
		os_timer_arm(&game_timer, (times[timer_index] * 60 *1000)/16, 1);
		system_os_task(user_scan_blob, procTaskPrio, procTaskQueue, procTaskQueueLen);
		system_os_post(procTaskPrio, 0, 0 );
	}
	begin_time -= 1;
}



int end_state = 0;
static void ICACHE_FLASH_ATTR end_game_func(void *arg)
{
	if(end_state % 2 == 0){
		make_lights(leds, 15, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 14, 0,0,0);
		make_lights(leds, 13, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 12, 0,0,0);
		make_lights(leds, 11, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 10, 0,0,0);
		make_lights(leds, 9, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 8, 0,0,0);

		make_lights(leds, 7, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 6, 0,0,0);
		make_lights(leds, 5, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 4, 0,0,0);
		make_lights(leds, 3, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 2, 0,0,0);
		make_lights(leds, 1, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 0, 0,0,0);
	}	else{
			make_lights(leds, 15, 0,0,0);
			make_lights(leds, 14, colors[state*3],colors[state*3+1],colors[state*3+2]);
			make_lights(leds, 13, 0,0,0);
			make_lights(leds, 12, colors[state*3],colors[state*3+1],colors[state*3+2]);
			make_lights(leds, 11, 0,0,0);
			make_lights(leds, 10, colors[state*3],colors[state*3+1],colors[state*3+2]);
			make_lights(leds, 9, 0,0,0);
			make_lights(leds, 8, colors[state*3],colors[state*3+1],colors[state*3+2]);

			make_lights(leds, 7, 0,0,0);
			make_lights(leds, 6, colors[state*3],colors[state*3+1],colors[state*3+2]);
			make_lights(leds, 5, 0,0,0);
			make_lights(leds, 4, colors[state*3],colors[state*3+1],colors[state*3+2]);
			make_lights(leds, 3, 0,0,0);
			make_lights(leds, 2, colors[state*3],colors[state*3+1],colors[state*3+2]);
			make_lights(leds, 1, 0,0,0);
			make_lights(leds, 0, colors[state*3],colors[state*3+1],colors[state*3+2]);
		}
	WS2812OutBuffer( leds, sizeof(leds), light_level );
	end_state = (end_state + 1) % 2;


}

//Called when new packet comes in.
static void ICACHE_FLASH_ATTR
udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;
	uart0_sendStr("X");
}

void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}

void make_lights(char light_array[], int light_num, int r, int g, int b){
  light_array[light_num*3] = g;
  light_array[light_num*3+1] = r;
  light_array[light_num*3+2] = b;
}

void scan_done( void	*arg,	STATUS	status );
void scan_done_blob( void	*arg,	STATUS	status );


void ICACHE_FLASH_ATTR change_state(void)
{
	printf("changing state, state is now %d\n",state);
	prev_state = state;
	struct softap_config softapconfig;
	wifi_softap_get_config(&softapconfig);

	memset(&softapconfig.ssid,	0,	sizeof(softapconfig.ssid));
	os_memcpy(&softapconfig.ssid, states[state], sizeof(states[state]));
	softapconfig.channel = 1;

	wifi_softap_set_config_current(&softapconfig);
	wifi_set_opmode(STATIONAP_MODE);
	wifi_softap_set_config_current(&softapconfig);
}



void ICACHE_FLASH_ATTR
init_game(void)
{
	system_os_task(init_game, procTaskInit, procTaskQueue, procTaskQueueLen);
	int buttons = GetButtons();
	 //208, no buttons
	 //221, A button
	 //210, B button
	 //223, both buttons


	if (!button_pressed && buttons == BBUTTON){
	 state = (state +1)%8;
	 button_pressed=true;
	 change_state();

	 }


	 if (button_pressed && buttons == NOBUTTONS){
		 button_pressed=false;
	 }

	 if (!button_pressed && buttons == ABUTTON){
		if (state == ZOMBIE || state == HUMAN || state == DEAD || state == SUPERZOMBIE){
	system_os_task(user_scan, procTaskPrio, procTaskQueue, procTaskQueueLen);
	 system_os_post(procTaskPrio, 0, 0 );}
	 else{
		 system_os_task(user_scan_blob, procTaskPrio, procTaskQueue, procTaskQueueLen);
			system_os_post(procTaskPrio, 0, 0 );
	 }
	}else{
		make_lights(leds, 0, colors[state*3],colors[state*3+1],colors[state*3+2]);
	WS2812OutBuffer( leds, sizeof(leds), light_level );
	 os_delay_us(1000);
	 system_os_post(procTaskInit, 0, 0 );
	}
}


int options_state = 3;

void ICACHE_FLASH_ATTR
game_options(void)
{
	system_os_task(game_options, procTaskopts, procTaskQueue, procTaskQueueLen);
	int buttons = GetButtons();

	state = HUMAN;

	if (options_state != 3)
	{
		state = NOTEAM;
	}

	if(options_state < 3){
	os_delay_us(1000);
	system_os_post(procTaskopts, 0, 0 );
}	else if(options_state == 3)
		{
			os_timer_disarm(&begin_timer);
			os_timer_setfn(&begin_timer, (os_timer_func_t *)begin_game_func, NULL);
			//was 45000 now 20000
			os_timer_arm(&begin_timer, 20, 1);
			begin_game_func(1);
			make_radar_full(leds, 1, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], 8);
			make_radar_full(leds, 0, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], 8);

		}else if(options_state == 4){
			os_timer_disarm(&begin_timer);
			os_timer_setfn(&begin_timer, (os_timer_func_t *)begin_game_func_blob, NULL);
			//was 45000 now 20000
			os_timer_arm(&begin_timer, 10, 1);
			begin_game_func_blob(1);
			make_radar_full(leds, 1, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], 8);
			make_radar_full(leds, 0, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], 8);



		}
	WS2812OutBuffer( leds, sizeof(leds), light_level );
}








void ICACHE_FLASH_ATTR
user_scan_blob(void)
{


		 int buttons = GetButtons();
	 	//208, no buttons
	 	//221, A button
	 	//210, B button
	 	//223, both buttons
		 if (!button_pressed && buttons == BOTHBUTTONS){
			if(light_level == 0x0f)
			{
				light_level = 0x01;
			}else if(light_level == 0x01)
			{
				light_level = 0x00;
			}else if(light_level == 0x00)
			{
				light_level = 0x30;
			}else if(light_level == 0x30)
			{
				light_level = 0x0f;
			}
	 		button_pressed=true;

	 	}

		if(prev_state != state)
		{
			change_state();
		}



	 	if (button_pressed && buttons == NOBUTTONS){

	 		button_pressed=false;
	 	}


	   struct	scan_config	config;

	   memset(&config,	0,	sizeof(config));
	   //config.ssid	=	"AI-THINKER_CC47D5";
	   config.channel = 1;

	   wifi_station_scan(&config,scan_done_blob);

}




void ICACHE_FLASH_ATTR
user_scan(void)
{
	if(end_game){
		os_timer_disarm(&end_timer);
		os_timer_setfn(&end_timer, (os_timer_func_t *)end_game_func, NULL);
		os_timer_arm(&end_timer, 500, 1);
	}else{
		 int buttons = GetButtons();
	 	//208, no buttons
	 	//221, A button
	 	//210, B button
	 	//223, both buttons


		if(buttons == BBUTTON)
		{
			button_pressed = true;
			show_time = true;
		}

		 if (!button_pressed && buttons == BOTHBUTTONS){
			if(light_level == 0x0f)
			{
				light_level = 0x01;
			}else if(light_level == 0x01)
			{
				light_level = 0x00;
			}else if(light_level == 0x00)
			{
				light_level = 0x30;
			}else if(light_level == 0x30)
			{
				light_level = 0x0f;
			}
	 		button_pressed=true;

	 	}

		if(prev_state != state)
		{
			printf("WOOP PREV STATE %d is not state %d changing\n",prev_state, state);
			change_state();
		}


	 	if (button_pressed && buttons == NOBUTTONS){
			if(show_time){
			show_time = false;
			make_radar_full(leds, 1, 0,0,0, 8);
			make_radar_full(leds, 0, 0,0,0, 8);

		}
	 		button_pressed=false;
	 	}


	   struct	scan_config	config;

	   memset(&config,	0,	sizeof(config));
	   //config.ssid	=	"AI-THINKER_CC47D5";
	   config.channel = 1;

	   wifi_station_scan(&config,scan_done);
	 }
}


void make_radar_gen(char leds[], int side, int r, int g, int b, int num, int max, int led_num){
	if(side == 0)
	{
		int a = led_num;
		for(a = led_num; a > 0; a = a - 1 ){

			if(led_num-a < (int)(num/(max/led_num))){
			make_lights(leds, a, r, g, b);
			}
			else{
			make_lights(leds,a,0,0,0);
			}
		}
	}

	else if(side == 1)
	{
		int a = 8;

		for(a = 8; a < 8+led_num; a = a + 1 ){
			if(a < (int)(num/(max/led_num)+8+1)){
			//printf("turns out it is\n");
			make_lights(leds, a, r, g, b);
			}
			else{
			//printf("turns out it isnt\n");
			make_lights(leds,a,0,0,0);
			}
		}
	}
}


void make_radar(char leds[], int side, int r, int g, int b, int num){
	if(side == 0)
	{
		int a = 7;
		for(a = 7; a > 0; a = a - 1 ){
			if(7-a < num){
			make_lights(leds, a, r, g, b);
			}
			else{
			make_lights(leds,a,0,0,0);
			}
		}
	}

	else if(side == 1)
	{
		int a = 8;
		for(a = 8; a < 15; a = a + 1 ){
			if(a < num+8){
			make_lights(leds, a, r, g, b);
			}
			else{
			make_lights(leds,a,0,0,0);
			}
		}
	}
}


void make_radar_full(char leds[], int side, int r, int g, int b, int num){
	if(side == 0)
	{
		int a = 7;
		for(a = 7; a >= 0; a = a - 1 ){
			if(7-a < num){
			make_lights(leds, a, r, g, b);
			}
			else{
			make_lights(leds,a,0,0,0);
			}
		}
	}

	else if(side == 1)
	{
		int a = 8;
		for(a = 8; a < 16; a = a + 1 ){
			if(a < num+8){
			make_lights(leds, a, r, g, b);
			}
			else{
			make_lights(leds,a,0,0,0);
			}
		}
	}
}



int get_radar_value(int values[], float distance)
{
	int a = 0;
	for(a = 0; a < 7; a = a + 1 ){
		if (distance > values[a]){
			continue;
		}else{
			return a;
		}
	}
	return 7;
}

int get_radar_value_full(int values[], float distance)
{
	int a = 0;
	for(a = 0; a < 8; a = a + 1 ){
		if (distance > values[a]){
			continue;
		}else{
			return a;
		}
	}
	return 8;
}


int cool_adding = 4200;

char macmap[15];


void scan_done_blob(void	*arg,	STATUS	status)
{
  system_os_task(user_scan_blob, procTaskPrio, procTaskQueue, procTaskQueueLen);
	uint8	ssid[33];
	char	temp[128];

	int closest_team_dist = -300;
	int closest_team = REDTEAM;


	if	(status	==	OK)	{
  	struct	bss_info	*bss_link	=	(struct	bss_info	*)arg;

  	while	(bss_link	!=	NULL)	{

  					memset(ssid,	0,	33);
  					if	(strlen(bss_link->ssid)	<=	32)
  						memcpy(ssid,	bss_link->ssid,	strlen(bss_link->ssid));
  					else
  						memcpy(ssid,	bss_link->ssid,	32);

            ets_sprintf(macmap, MACSTR, MAC2STR(bss_link->bssid));
            struct beacon_stat * beacon = hasht_get( hashtable, macmap );
            if (!beacon){
              int first_rssi[3] = {bss_link->rssi,bss_link->rssi,bss_link->rssi};

              struct beacon_stat b;
              b.type = ssid;
              b.rssi[0] = bss_link->rssi;
              b.rssi[1] = bss_link->rssi;
              b.rssi[2] = bss_link->rssi;

              ht_set( hashtable, macmap, &b );
              beacon = &b;
            }
            beacon->rssi[0] = beacon->rssi[1];
            beacon->rssi[1] = beacon->rssi[2];
            beacon->rssi[2] = bss_link->rssi;

            float average = (beacon->rssi[0] + beacon->rssi[1] + beacon->rssi[2])/3.0;


						if(strcmp(ssid, states[state]) == 0)
						{
							blob_health += (int)(100 + average);
							cool_adding += 1;
							if (average > closest_team_dist)
							{
								closest_team_dist = average;
								closest_team = state;
							}
						}else{

							if (average > closest_team_dist)
							{

								if (strcmp(ssid, states[REDTEAM]) == 0)
								{
									blob_health -= (100 + average);
									cool_adding += 1;
									closest_team_dist = average;
									closest_team = REDTEAM;
								}else if (strcmp(ssid, states[GREENTEAM]) == 0)
								{
									blob_health -= (100 + average);
									closest_team_dist = average;
									cool_adding += 1;
									closest_team = GREENTEAM;
								}else if (strcmp(ssid, states[BLUETEAM]) == 0)
								{
									blob_health -= (100 + average);
									cool_adding += 1;
									closest_team_dist = average;
									closest_team = BLUETEAM;
								}

							}
						}

  					bss_link	=	bss_link->next.stqe_next;
  	}
	}	else	{
					printf("scan	fail	!!!\r\n");
	}

		if (blob_health < 0 )
		{

			blob_health = 300;
			state = closest_team;

		}

		if (blob_health > 310 )
		{

			blob_health = 310;

		}

		int human_num = get_radar_value(zombie_health_inc, blob_health);
		make_radar(leds, 0, colors[state*3],colors[state*3+1],colors[state*3+2], 7);

		make_radar(leds, 1, colors[state*3],colors[state*3+1],colors[state*3+2], 7);

		make_lights(leds, 0, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 15, colors[state*3],colors[state*3+1],colors[state*3+2]);


	WS2812OutBuffer( leds, sizeof(leds), light_level );

  system_os_post(procTaskPrio, 0, 0 );
}



void scan_done(void	*arg,	STATUS	status)
{
  system_os_task(user_scan, procTaskPrio, procTaskQueue, procTaskQueueLen);
	uint8	ssid[33];
	char	temp[128];
	float closest_zombie = -300.0;
	float closest_dead = -300.0;

	//This is a human that is not nearby
	float closest_out_human = -300.0;
	float closest_human = -300.0;

	if	(status	==	OK)	{
  	struct	bss_info	*bss_link	=	(struct	bss_info	*)arg;

  	while	(bss_link	!=	NULL)	{

  					memset(ssid,	0,	33);
  					if	(strlen(bss_link->ssid)	<=	32)
  						memcpy(ssid,	bss_link->ssid,	strlen(bss_link->ssid));
  					else
  						memcpy(ssid,	bss_link->ssid,	32);

            ets_sprintf(macmap, MACSTR, MAC2STR(bss_link->bssid));
            struct beacon_stat * beacon = hasht_get( hashtable, macmap );
            if (!beacon){
              int first_rssi[3] = {bss_link->rssi,bss_link->rssi,bss_link->rssi};

              struct beacon_stat b;
              b.type = ssid;
              b.rssi[0] = bss_link->rssi;
              b.rssi[1] = bss_link->rssi;
              b.rssi[2] = bss_link->rssi;

              ht_set( hashtable, macmap, &b );
              beacon = &b;
            }
            beacon->rssi[0] = beacon->rssi[1];
            beacon->rssi[1] = beacon->rssi[2];
            beacon->rssi[2] = bss_link->rssi;

            float average = (beacon->rssi[0] + beacon->rssi[1] + beacon->rssi[2])/3.0;


						if(strcmp(ssid, states[ZOMBIE]) == 0 || strcmp(ssid, states[SUPERZOMBIE]) == 0)
						{
							if (average > closest_zombie)
							{
								closest_zombie = average;
							}
						}
						else if(strcmp(ssid, states[DEAD]) == 0)
						{
							if (average > closest_dead)
							{
								closest_dead = average;
							}
						}

						else if(strcmp(ssid, states[HUMAN]) == 0)
						{
							if (average > closest_out_human && average < -50)
							{
								closest_out_human = average;
							}
							if (average > closest_human)
							{
								closest_human = average;
							}
						}


  					bss_link	=	bss_link->next.stqe_next;
  	}
	}	else	{
					printf("scan	fail	!!!\r\n");
	}
		if (state == HUMAN )
		{

			if(closest_zombie > sensitivities[sensitivity_index]){
				state = ZOMBIE;
			}

			int zombie_num = get_radar_value(normal_radar, closest_zombie);
			make_radar(leds, 0, colors[ZOMBIE*3],colors[ZOMBIE*3+1],colors[ZOMBIE*3+2], zombie_num);

			make_radar(leds, 1, colors[ZOMBIE*3],colors[ZOMBIE*3+1],colors[ZOMBIE*3+2], zombie_num);

		}
		else if (state == DEAD){
				//do timer
				dead_amount += 2;

				int dist_num = get_radar_value(dead_amount_inc, dead_amount);
				make_radar(leds, 0, colors[DEAD*3],colors[DEAD*3+1],colors[DEAD*3+2], dist_num);
				make_radar(leds, 1, colors[DEAD*3],colors[DEAD*3+1],colors[DEAD*3+2], dist_num);
				if(dead_amount > 300){
					state = ZOMBIE;
				}
		}
		else if (state == ZOMBIE || state == SUPERZOMBIE)
		{


			int human_num = get_radar_value(normal_radar, closest_human);
			make_radar(leds, 0, colors[HUMAN*3],colors[HUMAN*3+1],colors[HUMAN*3+2], human_num);

			make_radar(leds, 1, colors[HUMAN*3],colors[HUMAN*3+1],colors[HUMAN*3+2], human_num);

		}

		make_lights(leds, 0, colors[state*3],colors[state*3+1],colors[state*3+2]);
		make_lights(leds, 15, colors[state*3],colors[state*3+1],colors[state*3+2]);


	if(show_time == true)
	{
		if(count_down_time>=8){

		make_radar_full(leds, 1, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], count_down_time-8);
		make_radar_full(leds, 0, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], 8);

	}else{
		make_radar_full(leds, 1, 0,0,0, 8);
		make_radar_full(leds, 0, colors[BLUETEAM*3],colors[BLUETEAM*3+1],colors[BLUETEAM*3+2], count_down_time);
	}
		WS2812OutBuffer( leds, sizeof(leds), light_level );
	}

	WS2812OutBuffer( leds, sizeof(leds), light_level );

  system_os_post(procTaskPrio, 0, 0 );
}


static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{

}

//THIS DOESN"T WORK
void HandleButtonEvent( uint8_t stat, int btn, int down )
{
	system_os_post(0, 0, 0 );
}

void user_init(void)
{


	state = 0;
	prev_state = 0;
	printf("start state, state is now %d\n",state);
  printf("SDK	version:%s\n",	system_get_sdk_version());
  printf("ESP8266	chip	ID:0x%x\n",	system_get_chip_id());

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart0_sendStr("\r\nesp82XX Web-GUI\r\n" VERSSTR "\b");

	char nameo[32] = "Human";
	struct softap_config softapconfig;
	wifi_softap_get_config(&softapconfig);

	memset(&softapconfig.ssid,	0,	sizeof(softapconfig.ssid));

	os_memcpy(&softapconfig.ssid, nameo, sizeof(nameo));
	softapconfig.channel = 1;

	wifi_softap_set_config_current(&softapconfig);
	wifi_set_opmode(STATIONAP_MODE);
	wifi_softap_set_config_current(&softapconfig);


	SetupGPIO();


	int buttons = GetButtons();

  hashtable = hasht_create( 300 );
  // wifi scan has to after system init done.

	if(buttons == NOBUTTONS){
  system_init_done_cb(game_options);
	}
	else if(buttons == BOTHBUTTONS){
		button_pressed = true;
		options_state = 4;
		system_init_done_cb(init_game);


	}	else if(buttons == BBUTTON){
			button_pressed = true;
			options_state = 4;
			system_init_done_cb(game_options);
		}
	//Timer example
	//os_timer_disarm(&some_timer);
	//os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	//os_timer_arm(&some_timer, 10000, 0);

  gpio_init();
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U,FUNC_GPIO2);
  GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);
  printf("ESP8266	chip ID:0x%x\n",	system_get_chip_id());

}


//There is no code in this project that will cause reboots if interrupts are disabled.
void EnterCritical() { }

void ExitCritical() { }
