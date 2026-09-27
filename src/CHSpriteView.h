#pragma once

#include <SPI.h>
#include "src/SD/SD.h"
#include <CHGfx.h>


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

/* Reads a packed 4bpp sprite with a 2-byte header (width, height) from `path` 
 * and blits it into the CHGfx framebuffer at (x, y). 
 *
 * File header structure:
 * - Byte 0: Width (w)
 * - Byte 1: Height (h)
 * Followed by h rows of ceil(w/2) bytes.
 *
 * Nothing is sent to the LCD here -- this only touches the in-RAM
 * framebuffer (gfx_blit). Call gfx_flush() yourself once you're done
 * drawing everything for the frame.
 *
 * transparentIndex: -1 (default) draws opaque; 0-15 skips pixels of that
 * palette index instead of drawing them.
 *
 * Returns 0 (SPRITE_OK) on success, or a negative error code on failure. */
int drawSpriteFile(const char *path, int x, int y, int transparentIndex = -1);

