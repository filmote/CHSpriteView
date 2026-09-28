#pragma once

#include <SPI.h>
#include "src/SD/SD.h"
#include <CHGfx.h>

/* Bytes read from the card per fileRead() call. As many whole rows as fit
 * are read at once and blitted before the next read, instead of one row
 * per SD access -- sdBegin() re-runs SD.begin() every call, which is the
 * real cost, so cutting how often it happens is what actually buys speed.
 * Must be at least as big as one row (GFX_W/2 = 64 bytes covers any width
 * up to a full screen). Override before including this header, e.g.
 * `#define SPRITE_BUF_SIZE 1024` -- bigger trades RAM for fewer reads. */
#ifndef SPRITE_BUF_SIZE
#define SPRITE_BUF_SIZE 512
#endif

/* Return status codes for drawSpriteFile */
enum SpriteResult {
    SPRITE_OK = 0,
    SPRITE_ERR_FILE_OPEN = -1,
    SPRITE_ERR_HEADER_READ = -2,
    SPRITE_ERR_BAD_DIMENSIONS = -3,
    SPRITE_ERR_TOO_WIDE = -4,
    SPRITE_ERR_TRUNCATED = -5
};


/* --------------------------------------------------------------------- */
/* SPI1 arbitration                                                       */
/* --------------------------------------------------------------------- */
/*
* The ST7735 and the microSD share SPI1 and want it configured differently.
* The SD side is self-healing: SPIClass::beginTransaction() calls spi_init(),
* which resets and reprograms SPI1 before every card operation. The LCD side
* is not, so spiClaimForLcd() puts the register back the way CHGfx's own
* spiInit() leaves it. Wrap every SD access in sdBegin()/sdEnd().
*
* sdBegin() waits for any in-flight frame DMA first: that DMA owns the bus,
* and the card would otherwise reprogram the peripheral out from under it.
*/
void spiClaimForLcd(void);
void sdBegin(void);
void sdEnd(void);

/* Reads a packed 4bpp sprite from `path` and blits it into the CHGfx
 * framebuffer at (x, y).
 *
 * If w and h are left at their default (-1), the file is expected to
 * carry a 2-byte header -- byte 0 width, byte 1 height -- and that header
 * is read and used. If both w and h are passed in (> 0), the header read
 * is skipped entirely and the file is treated as raw, headerless rows:
 * h rows of ceil(w/2) bytes, nothing else.
 *
 * Nothing is sent to the LCD here -- this only touches the in-RAM
 * framebuffer (gfx_blit). Call gfx_flush() yourself once you're done
 * drawing everything for the frame.
 *
 * transparentIndex: -1 (default) draws opaque; 0-15 skips pixels of that
 * palette index instead of drawing them.
 *
 * Returns 0 (SPRITE_OK) on success, or a negative error code on failure.
 * SPRITE_ERR_TOO_WIDE means the sprite's row doesn't fit in SPRITE_BUF_SIZE
 * bytes -- raise that constant, or narrow the sprite. */
int drawSpriteFile(const char *path, int x, int y, int transparentIndex = -1);
int drawSpriteFile(const char *path, int x, int y, int w, int h, int transparentIndex = -1);