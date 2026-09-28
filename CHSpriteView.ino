#include "src/CHGame.h"
#include "src/CHSpriteView.h"
#include "src/Enums.h"

#define FIRE_FRAMES 16
#define FIRE_X      0
#define FIRE_Y      64

CHGame chGame;

static SpriteFile spriteFiles[Images::Count];   /* Stores the starting block and other details of each image */
static uint8_t    fireIdx   = 0;

void setup() {
 
    Gfx.begin(GFX_DIV2, GFX_16BPP);
    Gfx.setPalette(palette, 16);

    chGame.setFrameRate(60);
    chGame.boot();

    Serial.waitForPC(1500);

    while (!loadAll(spriteFiles)) {
        /* No card or no frames: wait for START, then try again. */
        do { chGame.pollButtons(); delay(10); } while (!chGame.justPressed(START_BUTTON));
        // drawStaticScreen();
    }

    // drawStaticScreen();
}

void loop() {

    if (!chGame.nextFrame()) return;
    chGame.pollButtons();

    gfx_wait();
    Gfx.clear(Colors::Black);

    spriteDraw(spriteFiles[fireIdx], FIRE_X, FIRE_Y, -1);
    spriteDraw(spriteFiles[Images::Skull], 44, 50, 0);
    gfx_flushRectAsync(0, 0, 128, 128);  

    if (chGame.isFrameCount(8)) {
        if (++fireIdx >= FIRE_FRAMES) fireIdx = 0;
    }

}
