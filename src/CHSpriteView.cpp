#include "CHSpriteView.h"


/* Same SRAM-execution trick as CHGfx and Sd2Card: flash is 3 wait states
 * at 48 MHz, and the row copy/diff below is a tight byte loop. */
#define SV_RAMFUNC __attribute__((section(".srodata.svramfunc"), noinline))

/* The row consumer around rowCopyDiff (block -> rows -> framebuffer) can run
 * from SRAM too. It is only a few instructions per ROW, not per byte, so
 * this is a RAM-for-speed trade decided by measurement: in SRAM 233 fps,
 * in flash 231 fps (within noise), and flash saves 516 bytes of RAM. Only
 * the per-byte loop, rowCopyDiff, earns its place in SRAM. */
#ifndef SPRITE_CONSUMER_IN_RAM
    #define SPRITE_CONSUMER_IN_RAM 0
#endif
#if SPRITE_CONSUMER_IN_RAM
    #define SV_CONSUMER SV_RAMFUNC
#else
    #define SV_CONSUMER
#endif

#define SPRITE_BUF_SIZE 512

/* ------------------------------------------------------------------------ */
/* Draw context                                                             */
/* ------------------------------------------------------------------------ */
/*
 * The card delivers the file as a flat byte stream in 512-byte blocks; the
 * sprite is rows of rowBytes. Rows straddle block boundaries (the 2-byte
 * header alone guarantees that), so the consumer below reassembles rows:
 * whole rows are used in place in the block buffer, and only a row that is
 * split across two blocks is copied into `carry` first.
 *
 * All offsets are FILE offsets, so clipping, the header and the tail cache
 * are all just ranges of the same number line:
 *
 *   0          2                     startByte             endByte
 *   | header   | rows above screen   | visible rows ...    | rows below |
 */
struct DrawCtx {

    const SpriteFile *s;
    int       x, y;            /* screen position of the sprite            */
    int       transparent;     /* -1 opaque, else skipped palette index    */
    uint16_t  rowBytes;        /* ceil(w / 2)                              */
    uint16_t  row;             /* sprite row the next complete row is      */
    uint32_t  off;             /* file offset of the next byte fed in      */
    uint32_t  startByte;       /* first byte of the first visible row      */
    uint32_t  endByte;         /* one past the last byte of the last one   */
    bool      direct;          /* opaque byte copy into gfx_fb possible    */
    uint16_t  carryLen;        /* bytes of a split row gathered so far     */
    uint8_t   carry[128];      /* one row: 255 px max -> 128 bytes         */

};


// Render the supplied v(alue) into a character array.  Very low cost ..

static char *putU32(char *p, uint32_t v) {

    char tmp[10];
    uint8_t n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
    
}


// Concatenate p & s.  Very low cost ..

static char *putStr(char *p, const char *s) {

    while (*s) *p++ = *s++;
    *p = 0;
    return p;

}

/* Full-screen message for fatal problems (no card, no frames). */

static void fatal(const char *l1, const char *l2) {

    gfx_clear(Black);
    gfx_text(4, 50, l1, Colors::Red);
    if (l2) gfx_text(4, 62, l2, Colors::HUD_TEXT);
    gfx_text(4, 80, "START = retry", Colors::HUD_DIM);
    gfx_flush();

}


bool loadAll(SpriteFile (&spriteFiles)[Images::Count])
{
    /* Mount once. 24 MHz, stepping down to 12/6 MHz only if the card cannot
     * read its own boot sector reliably at speed. */
    if (!SD.begin(PIN_SD_CS)) {
        fatal("SD mount failed", "card? FAT16/32?");
        return false;
    }

    Sd2Card &card = SD.rawCard();

    /* Resolve every frame to its raw block address, once. */
    char path[] = "FIRE/FIRE_00.BIN";
    
    uint8_t fireCount = 0;
    int16_t bad = -1;
    int16_t fragmented = -1;

    for (uint8_t i = 0; i < Images::Count; i++) {

        path[10] = (char)('0' + i / 10);
        path[11] = (char)('0' + i % 10);
        int r = spriteLoad(spriteFiles[fireCount], path);

        if      (r == SPRITE_OK) fireCount++;
        else if (r == SPRITE_ERR_FRAGMENTED) {
            fragmented = i;
            break;
        }
        else {
            bad = i;
            break;
        }

    }

    if (fragmented > 0) {

        char partA[24];
        char partB[24];
        char *ptrA = partA;
        char *ptrB = partB;
        ptrA = putStr(ptrA, "FILE_");
        if (bad < 10) ptrA = putStr(ptrA, "0");
        putU32(partB, fragmented);
        ptrA = putStr(ptrA, ptrB);
        ptrA = putStr(ptrA, ".BIN");
        fatal("Fragmented file(s)", partA);
        return false;

    }

    else if (bad >= 0) {

        char partA[24];
        char partB[24];
        char *ptrA = partA;
        char *ptrB = partB;
        ptrA = putStr(ptrA, "FILE_");
        if (bad < 10) ptrA = putStr(ptrA, "0");
        putU32(partB, bad);
        ptrA = putStr(ptrA, ptrB);
        ptrA = putStr(ptrA, ".BIN");
        fatal("Bad file(s)", partA);
        return false;
    }

    else if (fireCount == 0) {
        fatal("No matching images", "/FIRE/FIRE_xx.BIN");
        return false;
    }

    return true;
}

/*
 * Copy one row into the framebuffer, touching only what differs.
 *
 * Scan in from the left for the first differing unit and in from the right
 * for the last, then copy just that span. The scans are the diff: an
 * unchanged row costs two comparisons per unit and no stores, and the
 * caller learns the changed column range for free. Returns false if the
 * row is identical to what is already there. *lo / *hi are BYTE offsets.
 *
 * MEASURED, NOT ASSUMED: the first version compared a byte at a time, on
 * the theory that it would hide inside the ~171 us each 512-byte block
 * takes to arrive by DMA. The on-device profile (-DSD_PROFILE) said
 * otherwise: ~220 us per block, about 20 cycles per byte - longer than the
 * DMA, so it became THE bottleneck of the whole frame. Hence word units.
 *
 * Alignment. The framebuffer row is word-aligned whenever x is a multiple
 * of 8. The source row sits wherever it falls in the 512-byte block
 * buffer: the fire files' 2-byte header puts every row at 2 mod 4. The
 * QingKe core does not promise cheap misaligned word loads, but a
 * misaligned WORD at an even address is two aligned HALFWORDS, so:
 *
 *   src % 4 == 0   plain word loads                    (1 load  / 4 bytes)
 *   src % 4 == 2   word assembled from two halfwords   (2 loads / 4 bytes)
 *   anything else  the original byte loop              (fallback)
 *
 * Word granularity means the dirty span is reported in 8-pixel steps,
 * which is exactly the 12 bpp flush alignment CHGfx rounds to anyway.
 */
static inline uint32_t ldWord(const uint8_t *p, bool half)
{
    if (!half) return *(const uint32_t *)p;
    const uint16_t *h = (const uint16_t *)p;
    return (uint32_t)h[0] | ((uint32_t)h[1] << 16);    /* little-endian */
}

// static SV_RAMFUNC bool rowCopyDiff(uint8_t *dst, const uint8_t *src, uint16_t n,
//                                    uint16_t *lo, uint16_t *hi)
static SV_RAMFUNC bool rowCopyDiff(uint8_t *dst, const uint8_t *src, uint16_t n)
{
    if ((((uintptr_t)dst | n) & 3) == 0 && ((uintptr_t)src & 1) == 0) {
        const bool half  = ((uintptr_t)src & 2) != 0;
        uint32_t  *d     = (uint32_t *)dst;
        const uint16_t words = n >> 2;

        uint16_t i = 0;
        while (i < words && ldWord(src + 4 * i, half) == d[i]) i++;
        if (i == words) return false;
        uint16_t j = words - 1;
        while (ldWord(src + 4 * j, half) == d[j]) j--;     /* stops at i at the latest */
        for (uint16_t k = i; k <= j; k++) d[k] = ldWord(src + 4 * k, half);
        return true;
    }

    /* Byte fallback: odd source address, or a width that is not a whole
     * number of words. */
    uint16_t i = 0;
    while (i < n && dst[i] == src[i]) i++;
    if (i == n) return false;
    uint16_t j = n - 1;
    while (dst[j] == src[j]) j--;
    // *lo = i;
    // *hi = j;
    for (uint16_t k = i; k <= j; k++) dst[k] = src[k];
    return true;
}

/* One complete sprite row has arrived: put it on screen (in the framebuffer). */
static SV_CONSUMER void emitRow(DrawCtx &c, const uint8_t *src)
{
    const int sy = c.y + c.row;
    c.row++;
    if (sy < 0 || sy >= GFX_H) return;     /* belt and braces: range is pre-clipped */

    const int w = c.s->w;

    if (c.direct) {
        /* Opaque, even x, even width, fully inside horizontally: the row's
         * bytes ARE the framebuffer bytes. */
        uint8_t *dst = gfx_fb + (uint32_t)sy * GFX_FB_STRIDE + (c.x >> 1);
        uint16_t lo, hi;
        // rowCopyDiff(dst, src, c.rowBytes, &lo, &hi));
        rowCopyDiff(dst, src, c.rowBytes);
    } else {
        /* Transparency, odd alignment or horizontal clipping: let CHGfx's
         * nibble-aware blit handle it, one row at a time. We cannot cheaply
         * tell what changed, so report the whole visible span. */
        gfx_blit(src, c.x, sy, w, 1, c.transparent);
        int x0 = c.x < 0 ? 0 : c.x;
        int x1 = c.x + w > GFX_W ? GFX_W : c.x + w;
        // dirtyAdd(c.dirty, x0, sy, x1);
    }
}

/*
 * Feed the next n bytes of the file (starting at file offset c.off).
 * Bytes before startByte (header, clipped rows, the unused front of the
 * first block) and after endByte are dropped; the rest is cut into rows.
 */
static SV_CONSUMER void consume(DrawCtx &c, const uint8_t *p, uint32_t n)
{
    /* Leading bytes we do not want. */
    if (c.off < c.startByte) {
        uint32_t k = c.startByte - c.off;
        if (k > n) k = n;
        p += k; n -= k; c.off += k;
    }
    /* Trailing bytes we do not want. */
    if (c.off >= c.endByte) {
        c.off += n;
        return;
    }
    if (c.off + n > c.endByte) n = c.endByte - c.off;
    c.off += n;

    while (n) {
        if (c.carryLen == 0 && n >= c.rowBytes) {
            emitRow(c, p);                 /* whole row in place: no copy */
            p += c.rowBytes;
            n -= c.rowBytes;
            continue;
        }
        /* Row split across a block boundary: gather it. */
        uint16_t k = c.rowBytes - c.carryLen;
        if (k > n) k = (uint16_t)n;
        memcpy(c.carry + c.carryLen, p, k);
        c.carryLen += k;
        p += k;
        n -= k;
        if (c.carryLen == c.rowBytes) {
            emitRow(c, c.carry);
            c.carryLen = 0;
        }
    }
}

/* readBlocksPipelined() callback: runs while the NEXT block is arriving. */
static SV_CONSUMER void onBlock(const uint8_t *block, void *user)
{
    consume(*static_cast<DrawCtx *>(user), block, 512);
}

/* ------------------------------------------------------------------------ */
/* Clipping / setup shared by both draw paths                                */
/* ------------------------------------------------------------------------ */
/* Returns false if nothing of the sprite is on screen. */
static bool setupCtx(DrawCtx &c, const SpriteFile &s, int x, int y,
                     int transparent)
{
    c.s           = &s;
    c.x           = x;
    c.y           = y;
    c.transparent = transparent;
    c.rowBytes    = (uint16_t)((s.w + 1) >> 1);
    c.carryLen    = 0;
    // c.dirty       = dirty;

    /* Fully off screen? */
    if (x >= GFX_W || y >= GFX_H || x + s.w <= 0 || y + s.h <= 0) return false;

    /* Visible rows [firstRow, lastRow). Rows above/below the screen are not
     * just skipped when drawing - they are left out of the byte range, so
     * the blocks that hold only them are never read from the card. */
    int firstRow = y < 0 ? -y : 0;
    int lastRow  = (y + s.h > GFX_H) ? GFX_H - y : s.h;
    c.row       = (uint16_t)firstRow;
    c.startByte = SPRITE_HEADER_BYTES + (uint32_t)firstRow * c.rowBytes;
    c.endByte   = SPRITE_HEADER_BYTES + (uint32_t)lastRow  * c.rowBytes;

    /* The fast path needs the row to be exactly a run of framebuffer bytes. */
    c.direct = transparent < 0 && !(x & 1) && !(s.w & 1) && x >= 0 && x + s.w <= GFX_W;
    return true;
}

/* ------------------------------------------------------------------------ */
/* spriteLoad                                                                */
/* ------------------------------------------------------------------------ */
int spriteLoad(SpriteFile &spriteFile, const char *path)
{
    spriteFile.w = spriteFile.h = 0;
    spriteFile.blocks = 0;
    spriteFile.tailLen = 0;

    File f = SD.open(path);
    if (!f) return SPRITE_ERR_FILE_OPEN;

    uint8_t header[SPRITE_HEADER_BYTES];
    if (f.read(header, SPRITE_HEADER_BYTES) != SPRITE_HEADER_BYTES) {
        f.close();
        return SPRITE_ERR_HEADER_READ;
    }
    const uint8_t w = header[0], h = header[1];
    if (w == 0 || h == 0) {
        f.close();
        return SPRITE_ERR_BAD_DIMENSIONS;
    }

    const uint32_t rowBytes = (w + 1u) >> 1;
    const uint32_t end      = SPRITE_HEADER_BYTES + rowBytes * h;   /* file bytes used */
    if (f.size() < end) {
        f.close();
        return SPRITE_ERR_TRUNCATED;
    }

    /* Where does it live? This walks the file's FAT chain once. */
    uint32_t first, last;
    if (!f.contiguousRange(first, last)) {
        f.close();
        return SPRITE_ERR_FRAGMENTED;
    }

    /* Blocks to stream, and whether the few bytes after the last whole
     * block can be cached instead of streaming one more block per draw. */
    const uint32_t full = end >> 9;
    const uint16_t rem  = (uint16_t)(end & 511u);
    if (rem == 0) {
        spriteFile.blocks = (uint16_t)full;
    } else if (rem <= SPRITE_TAIL_MAX && full > 0) {
        spriteFile.blocks = (uint16_t)full;
        if (!f.seek(full << 9) || f.read(spriteFile.tail, rem) != (int)rem) {
            f.close();
            return SPRITE_ERR_TRUNCATED;
        }
        spriteFile.tailLen = (uint8_t)rem;
    } else {
        spriteFile.blocks = (uint16_t)(full + 1);
    }

    f.close();
    spriteFile.firstBlock = first;
    spriteFile.w = w;
    spriteFile.h = h;
    return SPRITE_OK;
}

/* ------------------------------------------------------------------------ */
/* spriteDraw                                                                */
/* ------------------------------------------------------------------------ */
int spriteDraw(const SpriteFile &spriteFile, int x, int y, int transparent)
{
    if (spriteFile.w == 0) return SPRITE_ERR_NOT_LOADED;

    DrawCtx c;
    if (!setupCtx(c, spriteFile, x, y, transparent)) return SPRITE_OK;

    /* Everything below writes gfx_fb, and the card read borrows CHGfx's two
     * 512-byte flush chunk buffers as DMA landing space. Both are only safe
     * once the previous frame's async flush has finished reading them. */
    gfx_wait();

    /* Block range covering [startByte, endByte), limited to what is streamed;
     * anything past the streamed blocks comes from the tail cache. */
    const uint32_t streamEnd  = (uint32_t)spriteFile.blocks << 9;
    const uint32_t startBlk   = c.startByte >> 9;
    const uint32_t wantEnd    = c.endByte < streamEnd ? c.endByte : streamEnd;
    const uint32_t endBlk     = (wantEnd + 511u) >> 9;          /* exclusive */
    const uint16_t nBlk       = endBlk > startBlk ? (uint16_t)(endBlk - startBlk) : 0;
    c.off = startBlk << 9;

    if (nBlk) {
        uint8_t *buf0 = gfx_chunkScratch();
        uint8_t *buf1 = buf0 + GFX_CHUNK_BYTES;
        if (!SD.rawCard().readBlocksPipelined(spriteFile.firstBlock + startBlk, nBlk,
                                              buf0, buf1, onBlock, &c)) {
            return SPRITE_ERR_IO;
        }
    }

    /* Bytes beyond the last streamed block, from RAM. consume() ignores
     * them if the visible range ended earlier. */
    if (spriteFile.tailLen && c.endByte > streamEnd) {
        c.off = streamEnd;
        consume(c, spriteFile.tail, spriteFile.tailLen);
    }
    return SPRITE_OK;
}




// ----------------------------------------------------------------------
// Following is for drawing one sprite at a time ..


/* --------------------------------------------------------------------- */
/* SPI1 arbitration                                                      */
/* --------------------------------------------------------------------- */

#define SPI_MSTR (1u << 2)
#define SPI_SPE  (1u << 6)
#define SPI_SSI  (1u << 8)
#define SPI_SSM  (1u << 9)

/* Master, mode 0, software NSS held high, MSB first, 8-bit frames, BR = 000
 * (HCLK/2 = 24 MHz, the chip's ceiling and what gfx_begin(GFX_DIV2) asks for).
 * CTLR2 is cleared so the SD side cannot leave TXDMAEN or SSOE asserted;
 * CHGfx sets TXDMAEN itself per transfer and flips DFF as it needs it. */
void spiClaimForLcd(void)
{
    SPI1->CTLR1  = 0;
    SPI1->CTLR2  = 0;
    SPI1->CTLR1  = SPI_MSTR | SPI_SSM | SPI_SSI;
    SPI1->CTLR1 |= SPI_SPE;
}

void sdBegin(void) { gfx_wait(); }
void sdEnd(void)   { spiClaimForLcd(); }

static bool fileOpen(File &f, const char *path) {

    sdBegin();
    f = SD.open(path);
    sdEnd();
    return (bool)f;

}

static void fileClose(File &f) {

    sdBegin();
    f.close();
    sdEnd();

}

static void sdSeek(File *file, uint64_t pos) {

    File *f = file;
    sdBegin();
    f->seek(pos);
    sdEnd();

}

static bool fileRead(File &f, uint8_t *dst, size_t n) {

    sdBegin();
    const bool ok = ((size_t)f.read(dst, n) == n);
    sdEnd();
    return ok;

}

/* Does the actual work on an already-open file, using a caller-supplied
 * buffer of SPRITE_BUF_SIZE bytes. Never opens or closes anything, so
 * every error path can simply return. Reads from the file's current
 * position. */
static int drawSpriteCore(File &f, int x, int y, int w, int h, int transparentIndex, uint8_t *buf) {

    if (w <= 0 || h <= 0) {
        /* No size given -- the file must carry the 2-byte header. */
        uint8_t header[2];
        if (!fileRead(f, header, 2)) {
            return SPRITE_ERR_HEADER_READ;
        }
        w = header[0];
        h = header[1];
    }
    /* else: size was passed in, so the data is treated as raw rows with
     * no header at all. */

    if (w <= 0 || h <= 0) {
        return SPRITE_ERR_BAD_DIMENSIONS;
    }

    const size_t rowBytes = (size_t)(w + 1) / 2;
    if (rowBytes > SPRITE_BUF_SIZE) {
        return SPRITE_ERR_TOO_WIDE;
    }

    /* gfx_blit()'s signature always takes transparent as an int:
     *   void gfx_blit(const uint8_t *spr, int x, int y, int w, int h, int transparent);
     * -1 is assumed to mean "opaque, no transparent colour". */
    const int rowsPerRead = SPRITE_BUF_SIZE / rowBytes; /* >= 1, checked above */

    int row = 0;
    while (row < h) {
        const int rowsThisRead = (rowsPerRead < h - row) ? rowsPerRead : (h - row);
        const size_t bytesThisRead = rowsThisRead * rowBytes;

        if (!fileRead(f, buf, bytesThisRead)) {
            return SPRITE_ERR_TRUNCATED;
        }
        for (int i = 0; i < rowsThisRead; i++) {
            gfx_blit(buf + (size_t)i * rowBytes, x, y + row + i, w, 1, transparentIndex);
        }
        row += rowsThisRead;
    }

    return SPRITE_OK;
}


/* Shared body for every public overload. If `file` is non-null it is used
 * as-is (and left open); otherwise `path` is opened and closed here. */
static int drawSpriteImpl(File *file, const char *path, int x, int y, int w, int h, int transparentIndex) {

    File local;
    File *f = file;

    if (f) {
        if (!(*f)) {
            return SPRITE_ERR_FILE_OPEN;
        }
    } else {
        if (!path || !fileOpen(local, path)) {
            return SPRITE_ERR_FILE_OPEN;
        }
        f = &local;
    }

    uint8_t buf[SPRITE_BUF_SIZE];
    int result = drawSpriteCore(*f, x, y, w, h, transparentIndex, buf);

    // const int result = drawSpriteStackBuf(*f, x, y, w, h, transparentIndex);

    /* Only close what we opened -- a caller-supplied file stays open. */
    if (!file) {
        fileClose(local);
    }
    return result;
}

/* --- By path --------------------------------------------------------- */

int drawSpriteFile(const char *path, int x, int y, int transparentIndex) {
    return drawSpriteImpl(nullptr, path, x, y, -1, -1, transparentIndex);
}

int drawSpriteFile(const char *path, int x, int y, int w, int h, int transparentIndex) {
    return drawSpriteImpl(nullptr, path, x, y, w, h, transparentIndex);
}

/* --- By open File ---------------------------------------------------- */

int drawSpriteFile(File *file, int x, int y, int w, int h, uint8_t idx, int transparentIndex) {
    sdSeek(file, (w * h * idx) / 2);
    return drawSpriteImpl(file, nullptr, x, y, w, h, transparentIndex);
}
