/*
 * metroid.c
 * program which demonstrates sprites colliding with tiles
 */
 #include <stdio.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

/* include the background image we are using */
#include "background.h"

/* include the sprite image we are using */
#include "gba_sprites.h"

/* include the tile map we are using */
#include "map.h"
#include "map1.h"
//#include "map2.h"

/*startup screen*/
#include "GBA_Metroid_Title_Screen.h"
#include "GbaTitleScreenFinal.h"

/*mission complete screen*/
#include "MissionCompleteScreen.h"
#include "MissionCompleteMap.h"

/* Score background*/
#include "score_background.h"
/* using a manual map so text can be updated(the original tile map was a const)*/
unsigned short TextMap [32*32];


/**Assembly function declaration*/
int calc_offset(int offset, int tileWidth);
int samus_fall(int isFalling);

/*Hits and Lives*/
int numEnemies = 6;
int currentLife = 3;
 

/* the tile mode flags needed for display control register */
#define MODE0 0x00
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800


/* flags to set sprite handling in display control register */
#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000


/* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;
volatile unsigned short* bg3_control = (volatile unsigned short*) 0x400000e;


/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* there are 128 sprites on the GBA */
#define NUM_SPRITES 128

/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the memory location which controls sprite attributes */
volatile unsigned short* sprite_attribute_memory = (volatile unsigned short*) 0x7000000;

/* the memory location which stores sprite image data */
volatile unsigned short* sprite_image_memory = (volatile unsigned short*) 0x6010000;

/* the address of the color palettes used for backgrounds and sprites */
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;
volatile unsigned short* sprite_palette = (volatile unsigned short*) 0x5000200;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* scrolling registers for backgrounds */
volatile short* bg0_x_scroll = (unsigned short*) 0x4000010;
volatile short* bg0_y_scroll = (unsigned short*) 0x4000012;
volatile short* bg1_x_scroll = (unsigned short*) 0x4000014;
volatile short* bg1_y_scroll = (unsigned short*) 0x4000016;
volatile short* bg2_x_scroll = (unsigned short*) 0x4000018;
volatile short* bg2_y_scroll = (unsigned short*) 0x400001a;

/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank() {
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160) { }
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;

    /* if this value is zero, then it's not pressed */
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
    /* they are each 16K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
    /* they are each 2K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}

/* flag for turning on DMA */
#define DMA_ENABLE 0x80000000

/* flags for the sizes to transfer, 16 or 32 bits */
#define DMA_16 0x00000000
#define DMA_32 0x04000000

/* pointer to the DMA source location */
volatile unsigned int* dma_source = (volatile unsigned int*) 0x40000D4;

/* pointer to the DMA destination location */
volatile unsigned int* dma_destination = (volatile unsigned int*) 0x40000D8;

/* pointer to the DMA count/control */
volatile unsigned int* dma_count = (volatile unsigned int*) 0x40000DC;

/* copy data using DMA */
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
    *dma_source = (unsigned int) source;
    *dma_destination = (unsigned int) dest;
    *dma_count = amount | DMA_16 | DMA_ENABLE;
}

/* function to setup background 0 for this program */
void setup_background() {

    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) bg_palette, (unsigned short*) background_palette, PALETTE_SIZE);

    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) char_block(0), (unsigned short*) background_data,
            (background_width * background_height) / 2);

    /* set all control the bits in this register */
    *bg0_control = 2 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (21 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

    *bg1_control = 1 |
        (0 << 2)  |
        (0 << 6)  |
        (1 << 7)  |
        (22 << 8) |
        (1 << 13) |
        (0 << 14);

    /* load the tile data into screen block 16 */
    memcpy16_dma((unsigned short*) screen_block(21), (unsigned short*) map, map_width * map_height);
    memcpy16_dma((unsigned short*) screen_block(22), (unsigned short*) map1, map1_width * map1_height);
}

/* function for title background*/
/* function to setup background 0 for this program */
void setup_title_background() {

    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) bg_palette, (unsigned short*) GBA_Metroid_Title_Screen_palette, PALETTE_SIZE);

    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) char_block(0), (unsigned short*) GBA_Metroid_Title_Screen_data,
            (GBA_Metroid_Title_Screen_width * GBA_Metroid_Title_Screen_height) / 2);

    /* set all control the bits in this register */
    *bg0_control = 1 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (30 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

    

    /* load the tile data into screen block 16 */
    /* MAY HAVE TO CHANGE MAP NAME*/
    memcpy16_dma((unsigned short*) screen_block(30), (unsigned short*) GbaTitleScreenFinal, GbaTitleScreenFinal_width * GbaTitleScreenFinal_height);
}
/* function for mission complete background*/
/* function to setup background 0 for this program */
void setup_complete_background() {

    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) bg_palette, (unsigned short*) MissionCompleteScreen_palette, PALETTE_SIZE);

    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) char_block(0), (unsigned short*) MissionCompleteScreen_data,
            (MissionCompleteScreen_width * MissionCompleteScreen_height) / 2);

    /* set all control the bits in this register */
    *bg2_control = 1 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (21 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

    

    /* load the tile data into screen block 21 */
    /* MAY HAVE TO CHANGE MAP NAME*/
    memcpy16_dma((unsigned short*) screen_block(21), (unsigned short*) missionCompleteMap, missionCompleteMap_width * missionCompleteMap_height);
}
/* function for setting up the scoring tile*/
/* function to setup score background 0 for this program */
void setup_score_background() {

    /* have to use the original game palette, only one palette of 256 can be active at a time*/
    
   
    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) char_block(3), (unsigned short*) score_background_data,
            (score_background_width * score_background_height) / 2);

    /* set all control the bits in this register */
    *bg3_control = 0 |    /* priority, 0 is highest, 3 is lowest */
        (3 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (30 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

    

    /* using a manual textMap vs tile editor, because it needs to be updated*/
        

}
/* function to set text on the screen at a given location */
void set_text(char* str, int row, int col) {  
	/*clear previous text in textMap*/
	for(int i = 0; i < 32*32; i++){
		TextMap[i]=0;
	}                  
    /* find the index in the texmap to draw to */
    int index = row * 32 + col;

    /* the first 32 characters are missing from the map (controls etc.) */
    int missing = 32; 

    
    /* for each character */
    while (*str) {
        /* place this character in the map */
        TextMap[index] = *str - missing;

        /* move onto the next character */
        index++;
        str++;
    }
    /* need to call the DMA copy with the manual textMap*/   
    memcpy16_dma((unsigned short*) screen_block(30), (unsigned short*) TextMap, 32 * 32);

}




/* just kill time */
void delay(unsigned int amount) {
    for (int i = 0; i < amount * 10; i++);
}

/* a sprite is a moveable image on the screen */
struct Sprite {
    unsigned short attribute0;
    unsigned short attribute1;
    unsigned short attribute2;
    unsigned short attribute3;
};

/* array of all the sprites available on the GBA */
struct Sprite sprites[NUM_SPRITES];
int next_sprite_index = 0;

/* the different sizes of sprites which are possible */
enum SpriteSize {
    SIZE_8_8,
    SIZE_16_16,
    SIZE_32_32,
    SIZE_64_64,
    SIZE_16_8,
    SIZE_32_8,
    SIZE_32_16,
    SIZE_64_32,
    SIZE_8_16,
    SIZE_8_32,
    SIZE_16_32,
    SIZE_32_64
};

/* function to initialize a sprite with its properties, and return a pointer */
struct Sprite* sprite_init(int x, int y, enum SpriteSize size,
        int horizontal_flip, int vertical_flip, int tile_index, int priority) {

    /* grab the next index */
    int index = next_sprite_index++;

    /* setup the bits used for each shape/size possible */
    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    int h = horizontal_flip ? 1 : 0;
    int v = vertical_flip ? 1 : 0;

    /* set up the first attribute */
    sprites[index].attribute0 = y |             /* y coordinate */
        (0 << 8) |          /* rendering mode */
        (0 << 10) |         /* gfx mode */
        (0 << 12) |         /* mosaic */
        (1 << 13) |         /* color mode, 0:16, 1:256 */
        (shape_bits << 14); /* shape */

    /* set up the second attribute */
    sprites[index].attribute1 = x |             /* x coordinate */
        (0 << 9) |          /* affine flag */
        (h << 12) |         /* horizontal flip flag */
        (v << 13) |         /* vertical flip flag */
        (size_bits << 14);  /* size */

    /* setup the second attribute */
    sprites[index].attribute2 = tile_index |   // tile index */
        (priority << 10) | // priority */
        (0 << 12);         // palette bank (only 16 color)*/

    /* return pointer to this sprite */
    return &sprites[index];
}

/* update all of the spries on the screen */
void sprite_update_all() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

/* setup all sprites */
void sprite_clear() {
    /* clear the index counter */
    next_sprite_index = 0;

    /* move all sprites offscreen to hide them */
    for(int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].attribute0 = SCREEN_HEIGHT;
        sprites[i].attribute1 = SCREEN_WIDTH;
    }
}

/* set a sprite postion */
void sprite_position(struct Sprite* sprite, int x, int y) {
    /* clear out the y coordinate */
    sprite->attribute0 &= 0xff00;

    /* set the new y coordinate */
    sprite->attribute0 |= (y & 0xff);

    /* clear out the x coordinate */
    sprite->attribute1 &= 0xfe00;

    /* set the new x coordinate */
    sprite->attribute1 |= (x & 0x1ff);
}

/* move a sprite in a direction */
void sprite_move(struct Sprite* sprite, int dx, int dy) {
    /* get the current y coordinate */
    int y = sprite->attribute0 & 0xff;

    /* get the current x coordinate */
    int x = sprite->attribute1 & 0x1ff;

    /* move to the new location */
    sprite_position(sprite, x + dx, y + dy);
}

/* change the vertical flip flag */
void sprite_set_vertical_flip(struct Sprite* sprite, int vertical_flip) {
    if (vertical_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x2000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xdfff;
    }
}

/* change the vertical flip flag */
void sprite_set_horizontal_flip(struct Sprite* sprite, int horizontal_flip) {
    if (horizontal_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x1000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xefff;
    }
}

/* change the tile offset of a sprite */
void sprite_set_offset(struct Sprite* sprite, int offset) {
    /* clear the old offset */
    sprite->attribute2 &= 0xfc00;

    /* apply the new one */
    sprite->attribute2 |= (offset & 0x03ff);
}

/* setup the sprite image and palette */
void setup_sprite_image() {
    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) sprite_palette, (unsigned short*) gba_sprites_palette, PALETTE_SIZE);

    /* load the image into sprite image memory */
    memcpy16_dma((unsigned short*) sprite_image_memory, (unsigned short*) gba_sprites_data, (gba_sprites_width * gba_sprites_height) / 2);
}

/* a struct for Samus' logic and behavior */
struct Samus {
    /* the actual sprite attribute info */
    struct Sprite* sprite;

    /* the x and y postion in pixels */
    int x, y;

    /* Samus' y velocity in 1/256 pixels/second */
    int yvel;

    /* Samus' y acceleration in 1/256 pixels/second^2 */
    int gravity; 

    /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether Samus is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen Samus stays */
    int border;

    /* if Samus is currently falling */
    int falling;

    /* if Samus is facing backwards or not */ 
    int facing;
};

/* Struct for enemy */
struct Enemy {
    struct Sprite* sprite;
    int x;
    int y;
    int frame;
    int alive;
    int explosion;
};

/* struct for projectile */
struct Projectile {
    struct Sprite* sprite;
    int x;
    int dx;
    int y;
    int frame;
    int count;
    int alive;
};

/* initialize enemy sprites */
void enemy_init(struct Enemy* enemy, int x, int y, int frame) {
    enemy->x = x;
    enemy->y = y;
    enemy->alive = 1;
    enemy->explosion = 3;
    enemy->frame = frame;
    enemy->sprite = sprite_init(enemy->x, enemy->y, SIZE_16_32, 0, 0, enemy->frame, 1);
}

/* initialize projectile sprite */
void projectile_init(struct Projectile* projectile, struct Samus* samus, int frame) {
    
    projectile->y = samus->y;
    projectile->frame = frame;
    projectile->alive = 1;
    if (samus->facing) {
        projectile->x = samus->x - 8;
        projectile->dx = -2;
        sprite_set_horizontal_flip(projectile->sprite, 1);
    } else {
        projectile->x = samus->x + 8;
        projectile->dx = 2;
        sprite_set_horizontal_flip(projectile->sprite, 0);
    }
    if (projectile->count < 1) {
        projectile->count++;
        projectile->sprite = sprite_init(projectile->x, projectile->y, SIZE_16_32, 0, 0, projectile->frame, 0);
    }
}

/* initialize Samus */
void samus_init(struct Samus* samus) {
    samus->x = 50;
    samus->y = 113;
    samus->yvel = 0;
    samus->gravity = 30;
    samus->border = 40;
    samus->frame = 0;
    samus->move = 0;
    samus->counter = 0;
    samus->falling = 0;
    samus->facing = 0;
    samus->animation_delay = 8;
    samus->sprite = sprite_init(samus->x, samus->y, SIZE_16_32, 0, 0, samus->frame, 0);
}

/* move Samus left or right returns if it is at edge of the screen */
int samus_left(struct Samus* samus) {
    /* face left */
    sprite_set_horizontal_flip(samus->sprite, 1);
    samus->move = 1;
    
    /* to flip projectile */
    samus->facing = 1;

    /* if we are at the left end, just scroll the screen */
    if (samus->x < samus->border) {
        return 1;
    } else {
        /* else move left */
        samus->x--;
        return 0;
    }
}

int samus_right(struct Samus* samus) {
    /* face right */
    sprite_set_horizontal_flip(samus->sprite, 0);
    samus->move = 1;

    /* to flip projectile */
    samus->facing = 0;

    /* if we are at the right end, just scroll the screen */
    if (samus->x > (SCREEN_WIDTH - 16 - samus->border)) {
        return 1;
    } else {
        /* else move right */
        samus->x++;
        return 0;
    }
}

/* stop Samus from walking left/right */
void samus_stop(struct Samus* samus) {
    samus->move = 0;
    samus->frame = samus_fall(samus->falling);
   /* if (samus->falling) {
        samus->frame = 48;
    } else {
        samus->frame = 0;
    }*/
    samus->counter = 7;
    sprite_set_offset(samus->sprite, samus->frame);
}

/* start Samus jumping, unless already falling */
void samus_jump(struct Samus* samus) {
    if (!samus->falling) {
        samus->yvel = -1000;
        samus->falling = 1;
        samus->frame = 48;
        sprite_set_offset(samus->sprite, samus->frame);
    }
}

/* finds which tile a screen coordinate maps to, taking scroll into acco  unt */
unsigned short tile_lookup(int x, int y, int xscroll, int yscroll,
        const unsigned short* tilemap, int tilemap_w, int tilemap_h) {

    /* adjust for the scroll */
    x += xscroll;
    y += yscroll;

    /* convert from screen coordinates to tile coordinates */
    x >>= 3;
    y >>= 3;

    /* account for wraparound */
    while (x >= tilemap_w) {
        x -= tilemap_w;
    }
    while (y >= tilemap_h) {
        y -= tilemap_h;
    }
    while (x < 0) {
        x += tilemap_w;
    }
    while (y < 0) {
        y += tilemap_h;
    }

    /* the larger screen maps (bigger than 32x32) are made of multiple stitched
       together - the offset is used for finding which screen block we are in
       for these cases */
    int offset = 0;

    /* if the width is 64, add 0x400 offset to get to tile maps on right   */
    if (tilemap_w == 64 && x >= 32) {
        x -= 32;
        offset += 0x400;
    }

    /* if height is 64 and were down there */
    if (tilemap_h == 64 && y >= 32) {
        y -= 32;

        /*call the assembly function for the code that is commented out below*/
        offset= calc_offset(offset, tilemap_w);
        /* if width is also 64 add 0x800, else just 0x400 */
       /* if (tilemap_w == 64) {
            offset += 0x800;
        } else {
            offset += 0x400;
        }*/
    }

    /* find the index in this tile map */
    int index = y * 32 + x;

    /* return the tile */
    return tilemap[index + offset];
}

/* update Samus */
void samus_update(struct Samus* samus, int xscroll) {
    /* update  y position and speed if falling */
    if (samus->falling) {
        samus->y += (samus->yvel >> 8);
        samus->yvel += samus->gravity;
   }

    /* check which tile Samus' feet are over */
    unsigned short tile = tile_lookup(samus->x + 8, samus->y + 32, xscroll, 0, map1,
            map1_width, map1_height);

    /* if it's block tile
     * these numbers refer to the tile indices of the blocks the koopa can walk on */
    
    if ((tile >= 540 && tile <= 551) || (tile >= 556 && tile <= 569)) {
        /* stop the fall! */
        if(samus->falling) {
            samus->falling = 0;
        }
        samus->yvel = 0;
        

        /* make her line up with the top of a block works by clearing out the lower bits to 0 */
        samus->y &= ~0x3;

        /* move her down one because there is a one pixel gap in the image */
        samus->y++;

    } else {
        /* she is falling now */
        samus->falling = 1;        
    }

    /* update animation if moving */
    
    if (samus->move) {
        samus->counter++;
        if (samus->falling) {
            samus->frame = 48;

        } else if (samus->counter >= samus->animation_delay) {
            samus->frame = samus->frame + 16;
            if (samus->frame > 32) {
                samus->frame = 16;
            }
            sprite_set_offset(samus->sprite, samus->frame);
            samus->counter = 0;
        }
        if (!samus->falling) {
            samus->y++;
        }
        
    } else if(!samus->falling) {
        samus->y--;
    }

    /* set on screen position */
    sprite_position(samus->sprite, samus->x, samus->y);
}

void samus_falling(struct Samus* samus) {
    if (samus->falling) {
        samus->frame = 48;
        sprite_set_offset(samus->sprite, samus->frame);
    }
}

void enemy_move(struct Enemy* enemy1, struct Enemy* enemy2, struct Enemy* enemy3, struct Enemy* enemy4,
        struct Enemy* enemy5, struct Enemy* enemy6, int xscroll) {
    enemy1->x += xscroll;
    sprite_position(enemy1->sprite, enemy1->x, enemy1->y);
    
    enemy2->x += xscroll;
    sprite_position(enemy2->sprite, enemy2->x, enemy2->y);

    enemy3->x += xscroll;
    sprite_position(enemy3->sprite, enemy3->x, enemy3->y);

    enemy4->x += xscroll;
    sprite_position(enemy4->sprite, enemy4->x, enemy4->y);

    enemy5->x += xscroll;
    sprite_position(enemy5->sprite, enemy5->x, enemy5->y);

    enemy6->x += xscroll;
    sprite_position(enemy6->sprite, enemy6->x, enemy6->y);
} 

void clear_projectile(struct Projectile* projectile) {
        projectile->x = 125;
        projectile->y = -45;
        sprite_position(projectile->sprite, projectile->x, projectile->y);
        projectile->alive = 0;
        projectile->dx = 0; 
}

int enemy_hit(struct Projectile* projectile, struct Enemy* enemy) {
    
    if (projectile->x >= enemy->x && projectile->x <= enemy->x + 16) {
        if (projectile->y >= enemy->y && projectile->y <= enemy->y + 16){  
            clear_projectile(projectile);
            enemy->frame = 128;
            enemy->alive = 0;
            sprite_set_offset(enemy->sprite, enemy->frame);
            return 1;
        }
    }
    return 0;
}

void projectile_update(struct Projectile* projectile, struct Enemy* enemy1, struct Enemy* enemy2, struct Enemy* enemy3,
        struct Enemy* enemy4, struct Enemy* enemy5, struct Enemy* enemy6) {
    
    if (enemy_hit(projectile, enemy1)) {

    } else if (enemy_hit(projectile, enemy2)) {

    } else if (enemy_hit(projectile, enemy3)) {

    } else if (enemy_hit(projectile, enemy4)) {
    
    } else if (enemy_hit(projectile, enemy5)) {

    } else if (enemy_hit(projectile, enemy6)) {

    } else if (projectile->x + 12 == 0 || projectile->x + 12 == 1 || projectile->x == SCREEN_WIDTH || projectile->x == SCREEN_WIDTH - 1) {
        clear_projectile(projectile);

    } else if (projectile->alive) {
        projectile->x += projectile->dx;
        sprite_position(projectile->sprite, projectile->x, projectile->y);
    }   
}

/* Once the Number of Enemies or Number of Lives get to 0, game is over */
int playerWon = 0;
int isThereAWinner (){
	if(currentLife == 0){
		playerWon = 0;
		return 1;
	}
	else if (numEnemies == 0){
	    playerWon = 1;
	    return 1;
	}
	return 0; 
}

/*update hits and lives*/
void updateHitsandLives(){
	char msg[32];
	
	/* sprintf is printf for strings*/
	sprintf(msg, "Enemies: %d   Life: %d", numEnemies, currentLife);
	set_text(msg, 0,0);
}

void enemy_kill(struct Enemy* enemy) {
    if (enemy->explosion == 0){
        enemy->x = 55;
        enemy->y = -25;
        sprite_position(enemy->sprite, enemy->x, enemy->y);
        numEnemies--;
        enemy->explosion = 3;
        enemy->alive = 1;
        return;
    }
    enemy->explosion--;
}

void remove_enemies(struct Enemy* enemy1, struct Enemy* enemy2, struct Enemy* enemy3,
        struct Enemy* enemy4, struct Enemy* enemy5, struct Enemy* enemy6) {

    if (!enemy1->alive) {
        enemy_kill(enemy1);

    } else if (!enemy2->alive) {
        enemy_kill(enemy2);

    } else if (!enemy3->alive) {
        enemy_kill(enemy3);

    } else if (!enemy4->alive) {
        enemy_kill(enemy4);

    } else if (!enemy5->alive) {
        enemy_kill(enemy5);

    } else if (!enemy6->alive) {
        enemy_kill(enemy6);
    }
}

/* the main function */
int main() {

	/*FOR TITLE SCREEN*/
	/* we set the mode to mode 0 with bg0 on */
    *display_control = MODE0 | BG0_ENABLE;

    /* setup the background 0 */
    setup_title_background();
    /*forever loop for tile screen until 'A' is hit to start game*/
    while(1){
    	/* if START is pressed, break and go to game play */
        if (button_pressed(BUTTON_START)) {
            break;
        }
        /* wait for vblank */
        wait_vblank();
        
        /* delay some */
        delay(300);
    }

    /* we set the mode to mode 0 with bg0 on */
    *display_control = MODE0 | BG0_ENABLE | BG1_ENABLE | BG3_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;

    /* setup the background 0 */
    setup_background();
    
    /*setup score background*/
    setup_score_background();

    /* setup the sprite image data */
    setup_sprite_image();

    /* clear all the sprites on screen now */
    sprite_clear();

    /* create the koopa */
    struct Samus samus;
    samus_init(&samus);
    struct Projectile projectile;
    struct Enemy zeela;
    enemy_init(&zeela, 144, 1, 84);
    struct Enemy zeela2;
    enemy_init(&zeela2, 405, 1, 84);
    struct Enemy zombie;
    enemy_init(&zombie, 200, 119, 112);
    struct Enemy zombie2;
    enemy_init(&zombie2, 460, 119, 112);
    struct Enemy metroid;
    enemy_init(&metroid, 485, 48, 144);
    struct Enemy metroid2;
    enemy_init(&metroid2, 225, 48, 144);

    /* set initial scroll to 0 */
    int xscroll = 0;
    int xxscroll = 0;
    

    /* loop forever */
    while (1) {
        
        remove_enemies(&zeela, &zeela2, &zombie, &zombie2, &metroid, &metroid2);
        /* update Samus */
        samus_update(&samus, xxscroll);
        /* update projectile */
        projectile_update(&projectile, &zeela, &zeela2, &zombie, &zombie2, &metroid, &metroid2);

        /* now the arrow keys move the koopa */
        if (button_pressed(BUTTON_RIGHT)) {
            if (samus_right(&samus)) {
                xscroll++;
                xxscroll += 2;
                enemy_move(&zeela, &zeela2, &zombie, &zombie2, &metroid, &metroid2, -2);
            }
            
        } else if (button_pressed(BUTTON_LEFT)) {

            if (samus_left(&samus)) {
                xscroll--;
                xxscroll -= 2;
                enemy_move(&zeela, &zeela2, &zombie, &zombie2, &metroid, &metroid2, 2);
            }
            
        } else {
            samus_stop(&samus);
        }

        /* check for blaster */
        if (button_pressed(BUTTON_B)) {
             projectile_init(&projectile, &samus, 64);
        }

        /* check for jumping */
        if (button_pressed(BUTTON_A)) {
            samus_jump(&samus);
        }
        updateHitsandLives();
        
        samus_falling(&samus);
        /* wait for vblank before scrolling and moving sprites */
        wait_vblank();
        *bg0_x_scroll = xscroll;
        *bg1_x_scroll = xxscroll;
        sprite_update_all();
        
        /* delay some */
        delay(300);
        /*check to see if there is a winner */
        if(isThereAWinner()){
        	break;
        }
        
    }
    
    /* If the player wins the Mission Complete screen is shown */
    /* If player loses, the ending game screen stays */
 	/*set the mode to mode 0 with bg0 on*/
    *display_control = MODE0 | BG2_ENABLE | BG3_ENABLE;
    /*Mission complete screen*/
    setup_complete_background();
    setup_score_background();
    if (playerWon){
    	set_text("Mission Complete",10,5);
    }
    else{
    	set_text("Mission Failed",10,5);
    }

    while(1){
        wait_vblank();
        if(button_pressed(BUTTON_A)){
        	break;
        }
    }
}

