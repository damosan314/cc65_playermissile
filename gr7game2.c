
/*
 * gr7game2.c
 *
 * by: damosan / www.atariage.com
 *
 * A simple "game" using cc65.  It's actually far from a *real* game but
 * you're able to move a blue fighter and shoot two others.  AI?  What's
 * that?
 *
 * I did this to relearn basic graphics/PM stuff that I had all but 
 * forgotten from the 80s.  It was fun - hopefully sometime in the future
 * someone may find this useful as a place to start.
 *
 * This program uses:
 *   Graphics Mode 7 - stock display list.
 *   Four Players - overlapped to create two 2-color players.
 *   Bitmaps - uses a preshifted bitmap.  It's the only way CC65
 *       can keep up.  For my GR8 stuff I have a routine to generate
 *       all the shifts at init time.
 *
 * build:
 * cl65 --static-locals -m gr7game2.map -l gr7game2.lst -T -Osir 
 *     -t atari -C atari.cfg -o gr7game2.xex gr7game2.c
 */
#include <atari.h>
#include <peekpoke.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <conio.h>

typedef unsigned int  word;
typedef unsigned char byte;
typedef char          sbyte;

#define NMIEN  *((byte *) 0xd40e)
#define SDLSTL *((word *) 0x0230)
#define SDMCTL *((byte *) 0x022f)
#define DMACTL *((byte *) 0xd400)
#define RAMTOP *((byte *) 0x006a)
#define RAMSIZ *((byte *) 0x02e4)
#define WSYNC  *((byte *) 0xd40a)
#define SAVMSC *((word *) 0x0058)
#define COLOR0 *((byte *) 0x02c4)
#define COLOR1 *((byte *) 0x02c5)
#define COLOR2 *((byte *) 0x02c6)
#define COLOR3 *((byte *) 0x02c7)
#define COLBK  *((byte *) 0x02c8)
#define CHBAS  *((word *) 0x02f4)
#define RTCLOK *((byte *) 0x0012)
#define RT2    *((byte *) 0x0013)
#define RT3    *((byte *) 0x0014)
#define FCLR   *((word *) 0x00d4)
#define MEMTOP *((byte *) 0x02e5)
#define DLISTL *((word *) 0xd402)
#define RANDOM *((byte *) 0xd20a)
#define STICK0 *((byte *) 0x0278)
#define STRIG0 *((byte *) 0x0284) /* 0 when pressed, 1 when not */
#define ATRACT *((byte *) 0x004d)
#define GPRIOR *((byte *) 0x026f)
#define PMBASE *((word *) 0xd407)
#define GRACTL *((byte *) 0xd01d)
#define SIZEP0 *((byte *) 0xd008)
#define SIZEP1 *((byte *) 0xd009)
#define SIZEP2 *((byte *) 0xd00a)
#define SIZEP3 *((byte *) 0xd00b)
#define SIZEM  *((byte *) 0xd00c)
#define HPOSP0 *((byte *) 0xd000)
#define HPOSP1 *((byte *) 0xd001)
#define HPOSP2 *((byte *) 0xd002)
#define HPOSP3 *((byte *) 0xd003)
#define HPOSM0 *((byte *) 0xd004)
#define HPOSM1 *((byte *) 0xd005)
#define HPOSM2 *((byte *) 0xd006)
#define HPOSM3 *((byte *) 0xd007)
#define PCOLR0 *((byte *) 0xd012)
#define PCOLR1 *((byte *) 0xd013)
#define PCOLR2 *((byte *) 0xd014)
#define PCOLR3 *((byte *) 0xd015)
#define HITCLR *((byte *) 0xd01e)
#define GRAFP0 *((byte *) 0xd00d)
#define GRAFP1 *((byte *) 0xd00e)
#define GRAFP2 *((byte *) 0xd00f)
#define GRAFP3 *((byte *) 0xd010)
#define GRAFM  *((byte *) 0xd011)
#define VCOUNT *((byte *) 0xd40b)

#define EOK     0		/* returned by posix_memalign() */

#define MAX_ENEMIES 2
#define MAX_SHOTS   4

#define PMLOC1  1024		/* PMBASE+PMLOC1 */
#define PMSIZ1	10		/* size of player in bytes */
#define PMLOC2  1280
#define PMSIZ2  10
#define PMLOC3  1536
#define PMSIZ3  10
#define PMLOC4  1792
#define PMSIZ4  10

/*
 * page zero variables
 */
#define ZP_WORD  *((word *) 0xd4) /* FR0 - or floating point...so use it */
#define ZP_WORD1 *((word *) 0xd6) /* FR0 */
#define ZP_WORD2 *((word *) 0xd8) /* FR0 */
#define ZP_BYTE  *((byte *) 0xd6)
#define ZP_BYTE1 *((byte *) 0xd2) /* page zero reserved for cartridge use */
#define ZP_BYTE2 *((byte *) 0xd3) /* page zero reserved for cartridge use */
#define ZP_BYTE3 *((byte *) 0xda) /* FRE - float point register... */
#define ZP_BYTE4 *((byte *) 0xdb) /* FRE */
#define ZP_BYTE5 *((byte *) 0xdc) /* FRE */
#define ZP_BYTE6 *((byte *) 0xdd) /* FRE */
#define ZP_BYTE7 *((byte *) 0xde) /* FRE */
#define ZP_BYTE8 *((byte *) 0xdf) /* FRE */

#define ADDR         ZP_WORD
#define BINDEX       ZP_BYTE


#define JOYSTICK_NORTH      14
#define JOYSTICK_NORTHEAST   6
#define JOYSTICK_EAST        7
#define JOYSTICK_SOUTHEAST   5
#define JOYSTICK_SOUTH      13
#define JOYSTICK_SOUTHWEST   9
#define JOYSTICK_WEST       11
#define JOYSTICK_NORTHWEST  10

/*
 * player aircraft bitmap
 *
 * precalc the shifts required of the aircraft so we don't
 * have to do this at runtime.
 */
byte aircraft[ 4 ][ 21 ] = {
  { /* offset 0 */
    0b00000010, 0b00000000, 0b00000000,
    0b00000011, 0b10000000, 0b00000000,
    0b11000011, 0b11100000, 0b00000000,
    0b10111010, 0b10100110, 0b00000000,
    0b11000011, 0b11100000, 0b00000000,
    0b00000011, 0b10000000, 0b00000000,
    0b00000010, 0b00000000, 0b00000000,
  },
  { /* offset 1 */
    0b00000000, 0b10000000, 0b00000000,
    0b00000000, 0b11100000, 0b00000000,
    0b00110000, 0b11111000, 0b00000000,
    0b00101110, 0b10101001, 0b10000000,
    0b00110000, 0b11111000, 0b00000000,
    0b00000000, 0b11100000, 0b00000000,
    0b00000000, 0b10000000, 0b00000000,
  },
  { /* offset 2 */
    0b00000000, 0b00100000, 0b00000000,
    0b00000000, 0b00111000, 0b00000000,
    0b00001100, 0b00111110, 0b00000000,
    0b00001011, 0b10101010, 0b01100000,
    0b00001100, 0b00111110, 0b00000000,
    0b00000000, 0b00111000, 0b00000000,
    0b00000000, 0b00100000, 0b00000000,
  },
  { /* offset 3 */
    0b00000000, 0b00001000, 0b00000000,
    0b00000000, 0b00001110, 0b00000000,
    0b00000011, 0b00001111, 0b10000000,
    0b00000010, 0b11101010, 0b10011000,
    0b00000011, 0b00001111, 0b10000000,
    0b00000000, 0b00001110, 0b00000000,
    0b00000000, 0b00001000, 0b00000000,
  }
};

byte *color_array;
word gfx_indexes[ 96 ];
byte gfx_x_modulus[ 160 ];
byte gfx_shifted_colors[ 4 ][ 160 ];
bool first_screen = false;
word screens = 0;
byte bindex = 0;
word windex = 0;
byte temp_x, temp_y;
byte flipper = 0;
bool showing_explosion = 0;
byte tx = 0, ty = 0;
byte screen_counter = 0;

bool player_hit = false, e1hit = false, e2hit = false;
bool player_alive = true, e1alive = true, e2alive = true;
bool player_hidden = false;
byte player_x, player_y;
byte enemy_x[ MAX_ENEMIES ] = { 150, 150 };
byte enemy_y[ MAX_ENEMIES ] = { 20, 60 };
byte player_shots[ MAX_SHOTS * 2 ] = { 0, 0, 0, 0, 0, 0, 0, 0 };
byte enemy_shots[ MAX_SHOTS * 2 ] = { 0, 0, 0, 0, 0, 0, 0, 0 };
char *end_of_game_message = "Game over!\n\nThere are %d opponent(s) alive.\n\nHave a nice day.";

/*
 * pm variables
 */
byte p1pos = 0xc0;		/* player 1/2 horizontal position */
byte p3pos = 0xd0;		/* player 3/4 horizontal position */
word pm1offset;			/* offset of PM in memory area */
word pm3offset;			/* offset of PM in memory area */
word pmbase;			/* the base pointer for the P/M memory block */


void init_pm( void ) {
  void **ptr;
  byte pm;

  /*
   * wasteful of memory but it works.  The other approach is to
   * edit the linker file which sends people into fits.
   */
  if( EOK != posix_memalign( ptr, 1024, 2048))
    exit(0);

  pmbase = (word)ptr;
  pm = pmbase / 256;
  
  GRAFM = 0b11111111;
  SIZEM = 0b10101010;
  
  bzero( (word *)( pmbase + PMLOC1 ), 1024 );
  
  POKE( pmbase + PMLOC1 + 0, 0b00000000 );
  POKE( pmbase + PMLOC1 + 1, 0b00000000 );
  POKE( pmbase + PMLOC1 + 2, 0b00001000 );
  POKE( pmbase + PMLOC1 + 3, 0b00011000 );
  POKE( pmbase + PMLOC1 + 4, 0b01111100 );
  POKE( pmbase + PMLOC1 + 5, 0b01111100 );
  POKE( pmbase + PMLOC1 + 6, 0b00011000 );
  POKE( pmbase + PMLOC1 + 7, 0b00001000 );
  POKE( pmbase + PMLOC1 + 8, 0b00000000 );
  POKE( pmbase + PMLOC1 + 9, 0b00000000 );

  pm1offset = 70;
  memmove( (word *)( pmbase + PMLOC1 + pm1offset ),
	   (word *)( pmbase + PMLOC1 ),
	   PMSIZ1 );

  POKE( pmbase + PMLOC2 + 0, 0b00000000 );
  POKE( pmbase + PMLOC2 + 1, 0b00001000 );
  POKE( pmbase + PMLOC2 + 2, 0b00010000 );
  POKE( pmbase + PMLOC2 + 3, 0b00100001 );
  POKE( pmbase + PMLOC2 + 4, 0b10000010 );
  POKE( pmbase + PMLOC2 + 5, 0b10000000 );
  POKE( pmbase + PMLOC2 + 6, 0b00100010 );
  POKE( pmbase + PMLOC2 + 7, 0b00010001 );
  POKE( pmbase + PMLOC2 + 8, 0b00001000 );
  POKE( pmbase + PMLOC2 + 9, 0b00000000 );
  
  memmove( (word *)( pmbase + PMLOC2 + pm1offset ),
	   (word *)( pmbase + PMLOC2 ),
	   PMSIZ2 );
 
  POKE( pmbase + PMLOC3 + 0, 0b00000000 );
  POKE( pmbase + PMLOC3 + 1, 0b00000000 );
  POKE( pmbase + PMLOC3 + 2, 0b00001000 );
  POKE( pmbase + PMLOC3 + 3, 0b00011000 );
  POKE( pmbase + PMLOC3 + 4, 0b01111100 );
  POKE( pmbase + PMLOC3 + 5, 0b01111100 );
  POKE( pmbase + PMLOC3 + 6, 0b00011000 );
  POKE( pmbase + PMLOC3 + 7, 0b00001000 );
  POKE( pmbase + PMLOC3 + 8, 0b00000000 );
  POKE( pmbase + PMLOC3 + 9, 0b00000000 );
  
  pm3offset = 100;
  memmove( (word *)( pmbase + PMLOC3 + pm3offset ),
	   (word *)( pmbase + PMLOC3 ),
	   PMSIZ3 );
  POKE( pmbase + PMLOC3 + 0, 0b00000000 );
 
  POKE( pmbase + PMLOC4 + 0, 0b00000000 );
  POKE( pmbase + PMLOC4 + 1, 0b00001000 );
  POKE( pmbase + PMLOC4 + 2, 0b00010000 );
  POKE( pmbase + PMLOC4 + 3, 0b00100001 );
  POKE( pmbase + PMLOC4 + 4, 0b10000010 );
  POKE( pmbase + PMLOC4 + 5, 0b10000000 );
  POKE( pmbase + PMLOC4 + 6, 0b00100010 );
  POKE( pmbase + PMLOC4 + 7, 0b00010001 );
  POKE( pmbase + PMLOC4 + 8, 0b00001000 );
  POKE( pmbase + PMLOC4 + 9, 0b00000000 );
  
  memmove( (word *)( pmbase + PMLOC4 + pm3offset ),
	   (word *)( pmbase + PMLOC4 ),
	   PMSIZ4 );
  POKE( pmbase + PMLOC4 + 0, 0b00000000 );
  
  SDMCTL = 62;
  GRACTL = 0x03;
  PMBASE = (byte)(pmbase / 256);
  SIZEP0 = 0x00;		/* normal width (0x00), double (0x01), Quad (0x03) */
  SIZEP1 = 0x00;		/* normal width */
  SIZEP2 = 0x00;		/* normal width */
  SIZEP3 = 0x00;		/* normal width */
  PCOLR0 = 0x34;		/* ensure colors aren't messed with */
  PCOLR1 = 0x36;
  PCOLR2 = 0x34;
  PCOLR3 = 0x36;
  GPRIOR = 32;			/* blend players */
}


/*
 * void init_graphics_indexes()
 *
 */
void init_graphics_indexes( void ) {
  word depth;
  byte index;

  /*
   * store the address offsets of each line in a graphics 7 screen.
   * each line is 40 bytes long.
   */
  for( index = 0, depth = 0; index < 96; index++, depth += 40 )
    gfx_indexes[ index ] = SAVMSC + depth;
  
  /*
   * finally precompute the color masks for each color combination.
   * we do this for performance.  Looking up a value at runtime is
   * faster than calculating the value at runtime.  Time vs. Space
   * and all that.
   */
  for( index = 0; index < 160; index++ ) {
    gfx_x_modulus[ index ]           = (( index % 4 ) << 1 );
    gfx_shifted_colors[ 0 ][ index ] = 0b11000000 >> gfx_x_modulus[ index ];
    gfx_shifted_colors[ 1 ][ index ] = 0b10000000 >> gfx_x_modulus[ index ];
    gfx_shifted_colors[ 2 ][ index ] = 0b01000000 >> gfx_x_modulus[ index ];
    gfx_shifted_colors[ 3 ][ index ] = 0b00000000;
  }
}

long jiffies( void ) {
  long rv = (RTCLOK * 65536) + (RT2 * 256) + RT3;
  return rv;
}

void pause_jiffies( long t ) {
  long stop = jiffies() + t;
  while( jiffies() < stop )
    ;
}

void pause_one_jiffy( void ) {
  byte vh = RT2, vl = RT3 + 1;

  while( RT2 == vh && RT3 < vl )
    ;
}

void plot_sprite( byte x, byte y ) {
  byte *aircraft_array;
      
  ADDR           = gfx_indexes[ y ] + ( x >> 2 );
  aircraft_array = &aircraft[ x % 4 ][ 0 ];

  for( BINDEX = 0; BINDEX < 21; BINDEX += 3, ADDR += 40 ) {
    POKE( ADDR,     PEEK( ADDR     ) ^ aircraft_array[ BINDEX     ] );
    POKE( ADDR + 1, PEEK( ADDR + 1 ) ^ aircraft_array[ BINDEX + 1 ] );
    POKE( ADDR + 2, PEEK( ADDR + 2 ) ^ aircraft_array[ BINDEX + 2 ] );
  }
}

void climb( byte enemy ) {
  if( enemy == 1 ) {
    if( pm1offset == 50 )
      return;

    memmove( (word *)(pmbase + PMLOC1 + pm1offset - 1 ),
	     (word *)(pmbase + PMLOC1 + pm1offset ),
	     PMSIZ1 );
    memmove( (word *)(pmbase + PMLOC2 + pm1offset - 1 ),
	     (word *)(pmbase + PMLOC2 + pm1offset ),
	     PMSIZ2 );
    pm1offset -= 1;
  } else {
    if( pm3offset == 50 )
      return;

    memmove( (word *)(pmbase + PMLOC3 + pm3offset - 1 ),
	     (word *)(pmbase + PMLOC3 + pm3offset ),
	     PMSIZ3 );
    memmove( (word *)(pmbase + PMLOC4 + pm3offset - 1 ),
	     (word *)(pmbase + PMLOC4 + pm3offset ),
	     PMSIZ4 );
    pm3offset -= 1;
  }
}

void dive( byte enemy ) {
  if( enemy == 1 ) {
    if( pm1offset == 200 )
      return;
    memmove( (word *)(pmbase + PMLOC1 + pm1offset + 1),
	     (word *)(pmbase + PMLOC1 + pm1offset ),
	     PMSIZ1 );
    memmove( (word *)(pmbase + PMLOC2 + pm1offset + 1),
	     (word *)(pmbase + PMLOC2 + pm1offset ),
	     PMSIZ2 );
    ++pm1offset;
  } else {
    if( pm3offset == 200 )
      return;
    memmove( (word *)(pmbase + PMLOC3 + pm3offset + 1),
	     (word *)(pmbase + PMLOC3 + pm3offset ),
	     PMSIZ3 );
    memmove( (word *)(pmbase + PMLOC4 + pm3offset + 1),
	     (word *)(pmbase + PMLOC4 + pm3offset ),
	     PMSIZ4 );
    ++pm3offset;
  }  
}

void update_badguys( void ) {
  byte e1x             = p1pos - 44;
  byte e1y             = (pm1offset >> 1) - 15;
  byte e2x             = p3pos - 44;
  byte e2y             = (pm3offset >> 1) - 15;
  bool player_above_e1 = player_y < e1y;
  bool player_below_e1 = player_y > e1y;
  bool player_above_e2 = player_y < e2y;
  bool player_below_e2 = player_y > e2y;
  
  PCOLR0 = 0x34;		/* ensure colors aren't messed with */
  PCOLR1 = 0x36;
  PCOLR2 = 0x34;
  PCOLR3 = 0x36;

  if( e1alive ) {
    p1pos -= 1;
    if( p1pos < 44 )
      p1pos = 0xd0;

    if( player_x < e1x && player_y < e1y )
      dive( 1 );
    else
      climb( 1 );
    
  } else
    p1pos = 0x00;

  if( e2alive ) {
    p3pos -= 1;
    if( p3pos < 44 )
      p3pos = 0xd0;

    if(  player_x < e2x && player_y < e2y )
      dive( 2 );
    else
      climb( 2 );
    
  } else
    p3pos = 0x00;

  HPOSP0 = p1pos;
  HPOSP1 = p1pos;

  HPOSP2 = p3pos;
  HPOSP3 = p3pos;
}

void plot_shots( void ) {
  byte index;

  color_array = gfx_shifted_colors[ 2 ];
  
  for( index = 0; index < MAX_SHOTS << 1; index += 2 ) {
    tx = index;
    ty = index + 1;

    if( player_shots[ tx ] > 0 ) {
      ZP_WORD = gfx_indexes[ player_shots[ ty ]] + ( player_shots[ tx ] >> 2 );
      ZP_BYTE = color_array[ player_shots[ tx ]];
      *(byte *)ZP_WORD = *(byte *)ZP_WORD ^ ZP_BYTE;
    }
    
    if( enemy_shots[ tx ] > 0 ) {
      ZP_WORD = gfx_indexes[ enemy_shots[ ty ]] + ( enemy_shots[ tx ] >> 2 );
      ZP_BYTE = color_array[ enemy_shots[ tx ]];
      *(byte *)ZP_WORD = *(byte *)ZP_WORD ^ ZP_BYTE;
    }
  }    
}

#define PLAYER 1
#define ENEMY  2

void update_shots( byte type, byte shots[ MAX_SHOTS * 2 ] ) {
  byte index;
  
  for( index = 0; index < MAX_SHOTS << 1; index += 2 ) {
    tx = index;
    ty = index + 1;
    temp_x = shots[ tx ];
    temp_y = shots[ ty ];
    
    if( type == PLAYER ) {
      if( temp_x > 0 ) {
	temp_x += 3;
	if( temp_x > 159 ) {
	  temp_x = 0;
	  temp_y = 0;
	}
      }
    } else {
      if( temp_x >= 3 ) {
	temp_x -= 3;
	if( temp_x < 3 ) {
	  temp_x = 0;
	  temp_y = 0;
	}
      }
    }
    shots[ tx ] = temp_x;
    shots[ ty ] = temp_y;
  }
}

void player_shoot( void ) {
  byte index = 0;
  for( index = 0; index < MAX_SHOTS << 1; index += 2 ) {
    tx = index;
    ty = index + 1;
    if( player_shots[ tx ] == 0 ) {
      player_shots[ tx ] = player_x + 6;
      player_shots[ ty ] = player_y + 3;
      return;
    }
  }
}


void shoot( void ) {
  byte index = 0;
  
  if( RANDOM < 10 && e1alive )
    index = 1;

  if( RANDOM < 10 && index == 0 && e2alive )
    index = 2;
  
  switch( index ) {
  case 1:			/* first enemy shooting */
    temp_x = p1pos - 44;
    temp_y = (pm1offset >> 1) - 15;
    break;
  case 2:			/* second enemy shooting */
    temp_x = p3pos - 44;
    temp_y = (pm3offset >> 1) - 15;
    break;
  }

  if( temp_x % 2 != 0 )
    return;
  
  for( index = 0; index < MAX_SHOTS << 1; index += 2 ) {
    tx = index;
    ty = index + 1;
    
    if( enemy_shots[ tx ] == 0 ) {
      enemy_shots[ tx ] = temp_x;
      enemy_shots[ ty ] = temp_y;
      return;
    }
  }
}


/*
 * returns true if the player has released the fire button 
 */
bool update_player( void ) {
  static byte trigger_pressed = false;
  byte stick = STICK0;
  
  if( STRIG0 == 0 && trigger_pressed == false )
    trigger_pressed = true; 

  /*
   * automatic update because, you know, we're 
   * flying!
   */
  player_x += 1;
  if( player_x > 160 ) {
    player_x = 0;
    ++player_y;
    if( player_y == 89 ) {
      player_x = 0;
      player_y = 0;
    }
  }

  if( stick == JOYSTICK_NORTH ) {
    if( player_y > 0 )
      --player_y;
  } else if( stick == JOYSTICK_SOUTH ) {
    if( player_y < 90 )
      ++player_y;
  }
  
  /*
   * the player let go of the trigger!  Time to fire.
   */
  if( STRIG0 == 1 && trigger_pressed == true ) {
    trigger_pressed = false;
    return true;
  } else
    return false;
}


void hit_detect( void ) {
  byte index;
  byte player_zone_x      = player_x + 8;
  byte player_zone_y_low  = player_y;
  byte player_zone_y_high = player_y + 7;
  byte e1_zone_x          = p1pos - 48;
  byte e1_zone_x_high     = e1_zone_x + 8;
  byte e1_zone_y_low      = (pm1offset >> 1 ) - 18;
  byte e1_zone_y_high     = e1_zone_y_low + 8;
  byte e2_zone_x          = p3pos - 48;
  byte e2_zone_x_high     = e2_zone_x + 8;
  byte e2_zone_y_low      = (pm3offset >> 1 ) - 18;
  byte e2_zone_y_high     = e2_zone_y_low + 8;

  player_hit = false;
  e1hit      = false;
  e2hit      = false;

  for( index = 0; index < MAX_SHOTS << 1; index += 2 ) {
    tx = index;
    ty = index + 1;
    
    if( player_hit == false ) {
      if( enemy_shots[ tx ] == player_zone_x && enemy_shots[ ty ] >= player_zone_y_low && enemy_shots[ ty ] <= player_zone_y_high ) 
	player_hit = true;
    }

    if( e1hit == false ) {
      if( player_shots[ tx ] >= e1_zone_x &&
	  player_shots[ tx ] < e1_zone_x_high &&
	  player_shots[ ty ] >= e1_zone_y_low &&
	  player_shots[ ty ] <= e1_zone_y_high )
	e1hit = true;
    }

    if( e2hit == false ) {
      if( player_shots[ tx ] >=  e2_zone_x &&
	  player_shots[ tx ] < e2_zone_x_high &&
	  player_shots[ ty ] >= e2_zone_y_low &&
	  player_shots[ ty ] <= e2_zone_y_high )
	e2hit = true;
    }
  }
}
		     

int main( void ) {

  _graphics( 7 + 16 );
  init_pm();
  init_graphics_indexes();
  
  /* player position */
  player_x = 0;
  player_y = 00;

  /* playfield colors */
  COLOR0 = 0x34;
  COLOR1 = 0x74;
  COLOR2 = 0x72;
  COLBK  = 0x00;

  screen_counter = 0;
  
  /* game loop */
  while( player_alive && (e1alive || e2alive )) {
    ++screen_counter;

    flipper ^= 1;

    pause_one_jiffy();
    
    if( first_screen == true ) {
      plot_sprite( player_x, player_y );
      plot_shots();
    } else
      first_screen = true;

    if( update_player() )
      player_shoot();
    
    update_badguys();
    update_shots( PLAYER, player_shots );
    update_shots( ENEMY, enemy_shots );

    hit_detect();

    if( e1hit ) 
      e1alive = false;
    if( e2hit )
      e2alive = false;
    if( player_hit )
      player_alive = false;
    
    shoot();
    
    pause_one_jiffy();
    
    plot_sprite( player_x, player_y );
    plot_shots();
  }

  HPOSP0 = 0x00;
  HPOSP1 = 0x00;
  HPOSP2 = 0x00;
  HPOSP3 = 0x00;
  GRACTL = 0x00;		/* turn off p/m graphics */

  clrscr();
  printf(end_of_game_message, e2alive + e1alive );
  while( true )
    ;
  return 0;
}
