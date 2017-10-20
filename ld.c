/*
	ld.c
	A small SHUMP for the Game Boy <3
	sbarrio 2017
*/

#include <stdio.h>
#include <gb/gb.h>
#include <gb/font.h>
#include <gb/console.h>
#include <gb/drawing.h>
#include <rand.h>
#include <gb/hardware.h>


//Sprites
#include "ship.c"
#include "enemy.c"
#include "explosion.c"
#include "laser.c"
#include "title.c"	
#include "title.map"	

//System consts
#define SCR_WIDTH 160
#define SCR_HEIGHT 144

//Game consts
#define INI_PLAYER_X 75;
#define INI_PLAYER_Y 120;
#define PLAYER_HEALTH 1

#define SHIP_SPEED 6
#define ENEMY_SPEED 1
#define LASER_SPEED 2
#define BKG_SPEED  25

#define EXPLODING_TIMER 5
#define DEAD_TIMER 40	
#define RESPAWN_TIMER 20
#define INVINCIBLE_TIMER 10	
#define SHOOT_TIMER 35	


#define ID_ENEMY_HOMING 0	
#define ID_ENEMY_HOMING_1 4	
#define SCORE_ENEMY_HOMING 20	
#define MAX_Y_ENEMY_HOMING 80	
#define SCORE_EXTRA_LIFE 100 //Extra life every 255 enemies	

#define STATUS_ALIVE 0
#define STATUS_DYING 1	
#define STATUS_DEAD 2

#define NUM_ENEMIES 7  //MAX 10	(from indexes 2 - 22)
#define NUM_LASER 10   //MAX 10	(from indexes 24 - 44)

#define TITLE_DELAY 255	

//BKG data 
unsigned char myBkgData[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x36,0x00,0x08,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x50,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x0E,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0x00,0x20,0x00,0x50,0x00,0x00,0x00};


//variable declaration
font_t ibm_font;
UBYTE joy_status;
UBYTE lastKeys;
UINT8 start_pressed;
UINT8 frames;
UINT8 score;
UINT8 game_over;

//PLAYER
UINT8 x;
UINT8 y;
UINT8 lastTimeShot;
UINT8 health = PLAYER_HEALTH;

//ENEMY
UINT8 currentID;
UINT8 ex[NUM_ENEMIES];  // X
UINT8 ey[NUM_ENEMIES];	// Y
UINT8 es[NUM_ENEMIES];	// STATUS
UINT8 et[NUM_ENEMIES];	// TIMER

//LASER
UINT8 currentLaserID;
UINT8 lx[NUM_LASER];   // X
UINT8 ly[NUM_LASER];	// Y
UINT8 ls[NUM_LASER];	// STATUS
UINT8 lt[NUM_LASER];	// TIMER

//BG
UINT8 randomBkgTiles[20];

//function declaration
void setup();
void initTitleScreen();
void updateTitleScreen();
void initGame();
void updatePlayer();
void updateEnemies();
void updateLaser();
void shoot();
void updateHUD();
UINT8 overlap(UINT8 x1, UINT8 y1, UINT8 w1, UINT8 h1, UINT8 x2, UINT8 y2, UINT8 w2, UINT8 h2);
UINT8 getEnemySpriteIndex(UINT8 id);
UINT8 getEnemySpriteIndexFast(UINT8 id);
UINT8 getLaserSpriteIndexFast(UINT8 id);
void destroy(UINT8 index);
void killEnemy(UINT8 id);
void hurtPlayer();
void shootSound();
void explosionSound();
void startSound();
UINT8 generateNewXPosition();


void setup(){
	DISPLAY_ON; // TURNS ON THE GAMEBOY LCD

    font_init();
    ibm_font = font_load(font_ibm);
}

void initTitleScreen(){

	start_pressed = 0;

	set_bkg_data(0,64,tiledata);
	set_bkg_tiles(0,0,20,18,tilemap);

	//IMPORTANT - RELOAD THE FONT IN RAM, OTHERWISE TEXT WILL TAKE DATA FROM THE BKG WE JUST DRAWN 
    ibm_font = font_load(font_ibm);

	gotoxy(4,12);
	font_set(ibm_font);
    printf("-PUSH START-");

    gotoxy(4,17);
	font_set(ibm_font);
    printf("sbarrio 2017");

}

void updateTitleScreen(){
	if (joypad() == J_START){
		start_pressed = 1;
	}
}

void initGame(){

	UBYTE i;
	UBYTE j;

	initrand(DIV_REG); 

	//init game vars
	health = PLAYER_HEALTH;
	lastTimeShot = 0;
	game_over = 0;
	frames = 0;
	score = 0;

	//STATUS
	for (i=0; i!= NUM_ENEMIES; i++){
		ex[i] = 0;
		ey[i] = 0;
		es[i] = 0;
		et[i] = 0;
	}

	for (i=0; i!= NUM_LASER; i++){
		lx[i] = 0;
		ly[i] = 0;
		ls[i] = 0;
		lt[i] = 0;
	}
	
    //BG
	set_bkg_data(0, 4, myBkgData);
    for (j=0; j != 18; j++){
 
		for (i=0; i != 20; i++){
			randomBkgTiles[i] = rand() % 4;
		}
		set_bkg_tiles(0,j,20,1,randomBkgTiles); // SET A LINE OF BKG DATA (X, Y, W, H, DATA)
	}

    // initial player position
    x = INI_PLAYER_X;
	y = INI_PLAYER_Y;

	SPRITES_8x16;
	set_sprite_data(0, 8, ship);
	set_sprite_tile(0, 0);
	move_sprite(0,x, y);
	set_sprite_tile(1, 2);
	move_sprite(1, x + 8, y);

	//store enemy sprite data
	set_sprite_data(8, 8, enemy);
	currentID = 0;

	//Generate enemies
	for (i=0; i != NUM_ENEMIES; i++){

		currentID = getEnemySpriteIndexFast(i);

		if (i < 4){
			ex[i] = 30 * (i + 1);
			ey[i] = 20;			
		}else {
			ex[i] = 30 * (i - 4 + 1) + 15;
			ey[i] = 0;
		}

		set_sprite_tile(currentID, 8);
		move_sprite(2, ex[i],ey[i]);
		set_sprite_tile(currentID + 1, 10);
		move_sprite(3, ex[i] + 8, ey[i]);
	}

	set_sprite_data(16, 8, explosion);

	//store laser sprite data
	set_sprite_data(24, 8, laser);
	currentLaserID = 0;

	//Generate lasers
	for (i=0; i != NUM_LASER; i++){

		currentLaserID = getLaserSpriteIndexFast(i);

		lx[i] = SCR_WIDTH;
		ly[i] = 0;
		ls[i] = STATUS_DEAD;

		set_sprite_tile(currentLaserID, 24);
		move_sprite(4, lx[i],ly[i]);
		set_sprite_tile(currentLaserID + 1, 26);
		move_sprite(5, lx[i] + 8, ly[i]);
	}
			
	SHOW_SPRITES;

	updateHUD();

}

void updateTimers(){

	UBYTE i;

	for (i=0; i!= NUM_ENEMIES; i++){
		if (et[i] > 0){
			et[i] = et[i] - 1;	
		}
	}
}

void updatePlayer(){

		UBYTE i;

		//PLAYER INPUT
		joy_status = joypad();
		if (joypad() & J_B){
			shoot();
		}

		if(joy_status & J_UP)
			y--;
		if(joy_status & J_DOWN)
			y++;
		if(joy_status & J_LEFT)
			x--;
		if(joy_status & J_RIGHT)
			x++;

		//Screen boundaries
		if (x < 8){
			x = 8;
		}

		if (x > SCR_WIDTH - 8){
			x = SCR_WIDTH -8;
		}

		if (y < 16){
			y = 16;
		}

		if (y > SCR_HEIGHT){
			y = SCR_HEIGHT;
		}

		move_sprite(0,x,y); // move sprite 0 to x and y coords
		move_sprite(1,x + 8,y); // move sprite 0 to x and y coords
		delay(SHIP_SPEED);

		//COLLISIONS  (using a 8 by 8 square centered on the bottom of the ship's sprite)
		for (i = 0; i != NUM_ENEMIES; i++){
			if (es[i] == STATUS_ALIVE){
				if (overlap(x + 4, y + 8, 8, 8, ex[i], ey[i], 16, 16)){
					killEnemy(i);
					hurtPlayer();
				}				
			}
		}

		//shoot timer
		if (lastTimeShot > 0){
			lastTimeShot--;	
		}
}

UINT8 getEnemySpriteIndex(UINT8 id){
	return 2 * id + 2;
}

UINT8 getEnemySpriteIndexFast(UINT8 id){

	if (id == 0) return 2;
	if (id == 1) return 4;
	if (id == 2) return 6;
	if (id == 3) return 8;
	if (id == 4) return 10;
	if (id == 5) return 12;
	if (id == 6) return 14;
	if (id == 7) return 16;
	if (id == 8) return 18;
	if (id == 9) return 20;
	if (id == 10) return 22;
}

UINT8 getLaserSpriteIndexFast(UINT8 id){
	if (id == 0) return 24;
	if (id == 1) return 26;
	if (id == 2) return 28;
	if (id == 3) return 30;
	if (id == 4) return 32;
	if (id == 5) return 34;
	if (id == 6) return 36;
	if (id == 7) return 38;
	if (id == 8) return 40;
	if (id == 9) return 42;
	if (id == 10) return 44;
}

void updateEnemies(){

	UBYTE i;

	for (i = 0; i != NUM_ENEMIES; i++){

		currentID = getEnemySpriteIndexFast(i);

		if (es[i] == STATUS_ALIVE){
			ey[i] = ey[i] + ENEMY_SPEED;
			if (ey[i] > SCR_HEIGHT){
				ey[i] = 0;
				ex[i] = generateNewXPosition();
			}

			if (score > SCORE_ENEMY_HOMING && (i == ID_ENEMY_HOMING || i == ID_ENEMY_HOMING_1) && ey[i] < MAX_Y_ENEMY_HOMING) {
				//homing towards player
				if (ex[i] > x){
					ex[i]--;
				}

				if (ex[i] < x){
					ex[i]++;
				}
			}

			move_sprite(currentID,ex[i],ey[i]); // move sprite 0 to x and y coords
			move_sprite(currentID + 1 ,ex[i] + 8, ey[i]); // move sprite 0 to x and y coords
		}

		if (es[i] == STATUS_DYING){

			et[i]--;

			//Has it finished dying? -> reuse
			if (et[i] == 0){
				es[i] = STATUS_DEAD;
				et[i] = DEAD_TIMER;
			}
		}

		if (es[i] == STATUS_DEAD){
			et[i]--;
			if (et[i] == 0){
				es[i] = STATUS_ALIVE;
				et[i] = 0;
				ex[i] = generateNewXPosition();
				ey[i] = 10;

				move_sprite(currentID, ex[i] , ey[i] ); // move sprite 0 to x and y coords
				move_sprite(currentID + 1 ,ex[i] + 8, ey[i]); // move sprite 0 to x and y coords

				set_sprite_tile(currentID, 8);
				set_sprite_tile(currentID + 1, 10);
			}
		}
	}
}

void updateLaser(){

	UBYTE i;
	UBYTE j;

	for (i = 0; i != NUM_LASER; i++){

		currentLaserID = getLaserSpriteIndexFast(i);

		if (ls[i] == STATUS_ALIVE){
			ly[i] = ly[i] - LASER_SPEED;
			if (ly[i] <= 1){    //not 0 because laser moves 2 pixels at a time, meaning that when at 0 it already loops value (never reaches -1)
				ls[i] = STATUS_DEAD;
				move_sprite(currentLaserID,SCR_WIDTH,0); // move sprite 0 to x and y coords
				move_sprite(currentLaserID + 1 ,SCR_WIDTH + 8, 0); // move sprite 0 to x and y coords
			}else{
				move_sprite(currentLaserID,lx[i],ly[i]); // move sprite 0 to x and y coords
				move_sprite(currentLaserID + 1 ,lx[i] + 8, ly[i]); // move sprite 0 to x and y coords

				//COLLISIONS  (using a 8 by 8 square centered on the bottom of the ship's sprite)
				for (j = 0; j != NUM_ENEMIES; j++){
					if (es[j] == STATUS_ALIVE){
						if (overlap(lx[i] + 4, ly[i] + 8, 8, 8, ex[j], ey[j], 16, 16)){
							killEnemy(j);

							//resets laser
							ls[i] = STATUS_DEAD;
							move_sprite(currentLaserID,SCR_WIDTH,0); // move sprite 0 to x and y coords
							move_sprite(currentLaserID + 1 ,SCR_WIDTH + 8, 0); // move sprite 0 to x and y coords
						}				
					}
				}

			}
		}
	}
}

void updateHUD(){

	gotoxy(5,0);
	font_set(ibm_font);
    printf("SCORE - %d", score);
}

void shoot(){

	UINT8 i = 0;

	//able to shoot?
	if (lastTimeShot <= 0){

		//look for laser sprite available
		UINT8 ind = NUM_LASER;
		for (i=0; i<NUM_LASER; i++){
			if (ls[i] == STATUS_DEAD){
				ind = i;
				break;
			}
		}

		if (ind < NUM_LASER){
			lx[ind] = x;
			ly[ind] = y - 8;
			ls[ind] = STATUS_ALIVE;
			lastTimeShot = SHOOT_TIMER;

			shootSound();

		}else{
			//Have to wait until another laser is freed... :()
		}
	}
}

UINT8 overlap(UINT8 x1, UINT8 y1, UINT8 w1, UINT8 h1, UINT8 x2, UINT8 y2, UINT8 w2, UINT8 h2) {

	if ((x1 < (x2+w2)) && ((x1+w1) > x2) && (y1 < (h2+y2)) && ((y1+h1) > y2)) {
		return 1;
	} else {
		return 0;
	}

}

void destroy(UINT8 index){
	//set sprite to explosion (index and index +1)
	set_sprite_tile(index, 16);
	set_sprite_tile(index + 1, 18);

	explosionSound();
}

void killEnemy(UINT8 id){
		et[id] = EXPLODING_TIMER;
		es[id] = STATUS_DYING;
		currentID = getEnemySpriteIndexFast(id);
		destroy(currentID);

		if (score == SCORE_EXTRA_LIFE){
			score = 0;
			startSound();
			health += 1;
		}else{
			score += 1;	
		}

		updateHUD();
}

void hurtPlayer(){
	health--;
	if (health <= 0){
		destroy(0);
		game_over = 1;

		gotoxy(6,8);
		font_set(ibm_font);
    	printf("GAME OVER");

    	gotoxy(4,12);
		font_set(ibm_font);
    	printf("-PUSH START-");

	}
}

void shootSound(){

	NR52_REG = 0x8F; // TURN SOUND ON
	NR51_REG = 0x11; // ENABLE SOUND CHANNELS
	NR50_REG = 0x35; // VOLUME MAX = 0x77, MIN = 0x00

	NR10_REG = 0x1E;
	NR11_REG = 0x10;
	NR12_REG = 0xF3;
	NR13_REG = 0x00;
	NR14_REG = 0x87;
}

void explosionSound(){

	NR52_REG = 0x8F; // TURN SOUND ON
	NR51_REG = 0x11; // ENABLE SOUND CHANNELS
	NR50_REG = 0x40; // VOLUME MAX = 0x77, MIN = 0x00

	NR10_REG = 0x2A;
	NR11_REG = 0xC1;
	NR12_REG = 0x53;
	NR13_REG = 0x61;
	NR14_REG = 0xC6;
}

void startSound(){

	NR52_REG = 0x8F; // TURN SOUND ON
	NR51_REG = 0x11; // ENABLE SOUND CHANNELS
	NR50_REG = 0x40; // VOLUME MAX = 0x77, MIN = 0x00

	NR10_REG = 0x76;
	NR11_REG = 0x4D;
	NR12_REG = 0x87;
	NR13_REG = 0xA2;
	NR14_REG = 0x86;
}

UINT8 generateNewXPosition(){

	UBYTE s, r;

	r = rand();
    s = arand();

    if(r >= 8 && r <= 150 ) {
      	return r;
    }

    if (s >= 8 && s <= 150){
    	return s;
    }

    if (r - s > 100) {
    	return 120;
    }

    if (r + s < 100){
    	return 20;	
    }
    
    return 80;
}

void main(void){

	setup();
	initTitleScreen();

	while (!start_pressed){
		wait_vbl_done(); 
		updateTitleScreen();
	}

	startSound();
	delay(TITLE_DELAY);

	initGame();
		
	while(1){

		wait_vbl_done(); 

		//is game over?
		if (game_over == 1){

			start_pressed = 0;
			while (!start_pressed){
				if (joypad() == J_START){
					start_pressed = 1;
				}
				wait_vbl_done(); 
			}

			reset();
		}

		updatePlayer();
		updateEnemies();
		updateLaser();

		HIDE_WIN;
		SHOW_SPRITES; 
		SHOW_BKG;

 		lastKeys = joypad(); //keypresses from this vblank are stored for the next one to use if needed
	}
}
