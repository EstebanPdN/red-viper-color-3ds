// SPDX-License-Identifier: MIT
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rvc_video.h"
#include "vbcp.h"
#include "vb_dsp.h"
#include "vb_set.h"
#include "v810_mem.h"

enum { WIDTH = 384, HEIGHT = 224, VRAM_MASK = 0x3ffff, CACHE_SIZE = 256, MARKERS = 384 };
enum { COVERED = 0x01000000, FILLED = 0x02000000, PAINTED = 0x04000000, SHADE_SHIFT = 27 };
enum { NO_CELL = 65536, NO_GROUP = 256 };

typedef struct {
    u32 id;
    const u8 *rows;
    u16 marker;
    bool has_context;
} Character;

typedef struct {
    u32 colors[64];
    u32 cell;
    u16 attr, group;
    u8 world, shades;
    bool sprite;
    unsigned epoch;
} ColorTile;

typedef struct {
    s16 x, y;
    u16 group;
    u8 world, eye;
} Marker;

typedef struct {
    u32 pixel[2][HEIGHT][WIDTH];
    unsigned valid_eyes;
} Picture;

static Vbcp pack;
static u8 *pack_bytes;
static bool pack_in_linear;
static const char *pack_issue;
static bool display_oom;
static Picture *pictures;
static C3D_Tex display;
static Character chars[2048];
static ColorTile tiles[CACHE_SIZE];
static Marker markers[2][MARKERS];
static unsigned marker_count[2], recent;
static unsigned epoch;
static unsigned fade[64];
static const u8 *ram;
static const u32 auto_hues[] = {
    0x4ecdc4, 0xf4a261, 0x84cc66, 0xf17499, 0xa795ed, 0xe8ce64, 0x7bb9ff, 0xd0e2ee,
};

static u16 word(unsigned address) {
    address &= VRAM_MASK;
    return ram[address] | ((u16)ram[(address + 1) & VRAM_MASK] << 8);
}

static int sign_field(u16 word, unsigned bits) {
    unsigned sign = 1u << (bits - 1);
    return (int)((word & (sign - 1)) - (word & sign));
}

void rvc_reset(void) {
    display_oom = false;
    if (pictures) {
        pictures[0].valid_eyes = 0;
        pictures[1].valid_eyes = 0;
    }
    memset(marker_count, 0, sizeof(marker_count));
}

static void set_reference(unsigned reference) {
    for (unsigned n = 0; n < 64; n++) {
        float ratio = (float)n / reference;
        fade[n] = n >= reference ? 256 : (unsigned)lroundf(256.0f * powf(ratio, 1.0f / 2.2f));
    }
}

static void release_pack(void) {
    if (pack_in_linear) linearFree(pack_bytes);
    else free(pack_bytes);
    pack_bytes = NULL;
    pack_in_linear = false;
    memset(&pack, 0, sizeof(pack));
}

static bool take_pack(const char *path, u32 crc, u32 size) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END)) {
        pack_issue = "AUTO COLOR: PACK READ FAILED";
        fclose(file);
        return false;
    }
    long length = ftell(file);
    if (length < 12 || length > VBCP_MAX_BYTES || fseek(file, 0, SEEK_SET)) {
        pack_issue = "AUTO COLOR: INVALID PACK";
        fclose(file);
        return false;
    }

    u8 *bytes = malloc((size_t)length);
    bool linear = false;
    if (!bytes) {
        bytes = linearAlloc((size_t)length);
        linear = true;
    }
    if (!bytes) {
        pack_issue = "AUTO COLOR: PACK MEMORY LIMIT";
    } else if (fread(bytes, 1, length, file) != (size_t)length) {
        pack_issue = "AUTO COLOR: PACK READ FAILED";
    } else if (!vbcp_open(&pack, bytes, length, crc, size)) {
        pack_issue = "AUTO COLOR: INVALID OR MISMATCHED PACK";
    } else {
        pack_bytes = bytes;
        pack_in_linear = linear;
        set_reference(pack.reference);
        fclose(file);
        return true;
    }

    if (linear && bytes) linearFree(bytes);
    else free(bytes);
    fclose(file);
    return false;
}

void rvc_load(const char *rom_path, u32 crc, u32 size) {
    rvc_reset();
    release_pack();
    pack_issue = NULL;
    set_reference(63);
    char path[320];
    const char *slash = strrchr(rom_path, '/');
    const char *extension = strrchr(slash ? slash + 1 : rom_path, '.');
    size_t stem = extension ? (size_t)(extension - rom_path) : strlen(rom_path);
    if (stem && stem + sizeof(".vbcp") <= sizeof(path)) {
        snprintf(path, sizeof(path), "%.*s.vbcp", (int)stem, rom_path);
        if (take_pack(path, crc, size)) return;
    }
    static const char *dirs[] = {"sdmc:/red-viper-color/colorpacks", "sdmc:/red-viper/colorpacks"};
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(*dirs); i++) {
        snprintf(path, sizeof(path), "%s/%08lX.vbcp", dirs[i], (unsigned long)crc);
        if (take_pack(path, crc, size)) return;
    }
}

const char *rvc_pack_status(void) {
    if (!tVBOpt.FULL_COLOR) return NULL;
    if (tVBOpt.ANAGLYPH || is_multiplayer) return "USING ORIGINAL RENDERER";
    if (display_oom) return "FULL COLOR MEMORY LIMIT";
    if (pack_bytes) return "SD PALETTE LOADED";
    return pack_issue ? pack_issue : "AUTOMATIC COLOR";
}

bool rvc_active(void) {
    return tVBOpt.FULL_COLOR && !tVBOpt.ANAGLYPH && !is_multiplayer;
}

static bool buffers(void) {
    if (pictures) return true;
    if (display_oom) return false;
    Picture *storage = linearAlloc(2 * sizeof(*storage));
    if (!storage) {
        display_oom = true;
        return false;
    }
    if (!C3D_TexInit(&display, 512, 512, GPU_RGBA8)) {
        linearFree(storage);
        display_oom = true;
        return false;
    }
    C3D_TexSetFilter(&display, GPU_NEAREST, GPU_NEAREST);
    memset(storage, 0, 2 * sizeof(*storage));
    pictures = storage;
    return true;
}

void rvc_quit(void) {
    rvc_reset();
    release_pack();
    if (pictures) {
        C3D_TexDelete(&display);
        linearFree(pictures);
        pictures = NULL;
    }
}

static const Character *character(unsigned index) {
    Character *ch = &chars[index];
    if (!ch->rows) {
        unsigned bank = index >> 9;
        ch->rows = ram + 0x6000 + bank * 0x8000 + (index & 511) * 16;
        ch->id = vbcp_tile_id(ch->rows);
        ch->marker = NO_GROUP;
        ch->has_context = vbcp_has_context(&pack, ch->id);
        for (unsigned group = 0; group < pack.group_count; group++) {
            if (vbcp_marker(&pack, group, ch->id)) {
                ch->marker = group;
                break;
            }
        }
    }
    return ch;
}

static unsigned context_at(const Character *ch, int x, int y, unsigned eye, unsigned world, bool sprite) {
    unsigned next = recent ^ 1;
    if (ch->marker != NO_GROUP && marker_count[next] < MARKERS) {
        bool layer_bound = vbcp_layer_group(&pack, ch->marker);
        if (sprite != layer_bound) {
            unsigned n = marker_count[next];
            Marker *m = &markers[next][n];
            if (!n || m[-1].world != world || m[-1].eye != eye || m[-1].group != ch->marker ||
                abs(m[-1].x - x) + abs(m[-1].y - y) >= 8) {
                *m = (Marker){x, y, ch->marker, world, eye};
                marker_count[next]++;
            }
        }
    }
    if (!ch->has_context || !marker_count[recent]) return NO_GROUP;
    unsigned group = NO_GROUP;
    int nearest = 49;
    for (unsigned i = 0; i < marker_count[recent]; i++) {
        const Marker *m = &markers[recent][i];
        if (m->world != world || m->eye != eye) continue;
        int distance = abs(m->x - x) + abs(m->y - y);
        if (distance >= nearest) continue;
        if (!vbcp_paint(&pack, VBCP_CONTEXT, ch->id, m->group, 0).mask) continue;
        nearest = distance;
        group = m->group;
    }
    return group;
}

static u32 rgb_at(const uint8_t *rgb, unsigned pixel) {
    const u8 *p = rgb + 3 * pixel;
    return ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
}

static void fill_colors(ColorTile *tile, const Character *ch, unsigned world, unsigned attr,
                        VbcpPaint tuned, unsigned group, bool sprite, unsigned shades) {
    unsigned palette = attr >> 14;
    VbcpPaint rules[] = {tuned,
        group == NO_GROUP ? (VbcpPaint){0} : vbcp_paint(&pack, VBCP_CONTEXT, ch->id, group, 0),
        vbcp_paint(&pack, VBCP_PALETTE, ch->id, palette, 0),
        vbcp_paint(&pack, VBCP_LAYER, ch->id, world, 0),
        vbcp_paint(&pack, VBCP_TILE, ch->id, 0, 0)};
    uint64_t keep = vbcp_paint(&pack, VBCP_KEEP, ch->id, 0, 0).keep;
    u32 hue = auto_hues[(world + (sprite ? 3 + palette : 0)) % 8];

    for (unsigned y = 0; y < 8; y++) {
        unsigned row = ch->rows[y * 2] | (ch->rows[y * 2 + 1] << 8);
        for (unsigned x = 0; x < 8; x++) {
            unsigned index = y * 8 + x;
            uint64_t bit = UINT64_C(1) << index;
            unsigned raw = (row >> (2 * x)) & 3;
            unsigned shade = (shades >> (2 * raw)) & 3;
            u32 color = raw ? COVERED | (shade << SHADE_SHIFT) | hue : 0;
            if (!raw && (tuned.mask & bit) && !(tuned.keep & bit))
                color = FILLED | PAINTED | rgb_at(tuned.rgb, index);
            if (raw && shade && !(tuned.keep & bit)) {
                for (unsigned r = 0; r < sizeof(rules) / sizeof(*rules); r++) {
                    if (r == 4 && (keep & bit)) break;
                    if (rules[r].mask & bit) {
                        color = COVERED | PAINTED | (shade << SHADE_SHIFT) | rgb_at(rules[r].rgb, index);
                        break;
                    }
                }
            }
            tile->colors[index] = color;
        }
    }
}

static const ColorTile *resolve(unsigned attr, unsigned cell, unsigned world, bool sprite,
                                unsigned eye, int x, int y) {
    const Character *ch = character(attr & 2047);
    unsigned group = context_at(ch, x, y, eye, world, sprite);
    VbcpPaint tuned = cell == NO_CELL ? (VbcpPaint){0} :
        vbcp_paint(&pack, VBCP_CELL, ch->id, attr >> 14, cell);
    if (!(tuned.mask | tuned.keep)) cell = NO_CELL;
    unsigned shades = (sprite ? vb_state->tVIPREG.JPLT : vb_state->tVIPREG.GPLT)[attr >> 14];
    unsigned index = (attr * 31u + cell * 59u + world * 13u + group * 17u) % CACHE_SIZE;
    ColorTile *tile = &tiles[index];
    if (tile->epoch == epoch && tile->attr == attr && tile->cell == cell &&
        tile->world == world && tile->sprite == sprite && tile->shades == shades && tile->group == group)
        return tile;
    tile->attr = attr;
    tile->cell = cell;
    tile->world = world;
    tile->sprite = sprite;
    tile->shades = shades;
    tile->group = group;
    tile->epoch = epoch;
    fill_colors(tile, ch, world, attr, tuned, group, sprite, shades);
    return tile;
}

static inline void overlay(u32 *dst, u32 value) {
    if ((value & COVERED) || ((value & FILLED) && !(*dst & (COVERED | FILLED)))) *dst = value;
}

static unsigned map_cell(unsigned base, unsigned maps_x, unsigned maps_y, bool over,
                         unsigned outside, int u, int v) {
    unsigned width = maps_x * 512, height = maps_y * 512;
    if (over && ((unsigned)u >= width || (unsigned)v >= height)) return outside;
    unsigned x = (unsigned)u & (width - 1), y = (unsigned)v & (height - 1);
    unsigned map = base + x / 512 + y / 512 * maps_x;
    return (map * 4096 + ((y & 511) / 8) * 64 + ((x & 511) / 8)) & 65535;
}

static void draw_normal(Picture *picture, unsigned eye, unsigned layer, const WORLD *w,
                        int left, int top, int right, int bottom, int sx, int sy) {
    unsigned maps_x = 1u << w->scx, maps_y = 1u << w->scy;
    for (int y = top; y < bottom;) {
        int rows = 8 - (sy & 7);
        if (rows > bottom - y) rows = bottom - y;
        for (int x = left, u = sx; x < right;) {
            int columns = 8 - (u & 7);
            if (columns > right - x) columns = right - x;
            unsigned cell = map_cell(w->bg_map_base, maps_x, maps_y, w->is_over, w->over & 2047, u, sy);
            unsigned attr = word(BGMAP_OFFSET + 2 * cell);
            const ColorTile *tile = resolve(attr, cell, layer, false, eye, x, y);
            unsigned flip_x = attr & 0x2000 ? 7 : 0, flip_y = attr & 0x1000 ? 7 : 0;
            for (int row = 0; row < rows; row++) {
                u32 *dest = &picture->pixel[eye][y + row][x];
                const u32 *colors = tile->colors + ((((sy + row) & 7) ^ flip_y) * 8);
                for (int col = 0; col < columns; col++)
                    overlay(dest + col, colors[((u + col) & 7) ^ flip_x]);
            }
            x += columns;
            u += columns;
        }
        y += rows;
        sy += rows;
    }
}

static void draw_background(Picture *picture, unsigned eye, unsigned layer, const WORLD *w) {
    int side = eye ? 1 : -1;
    int x0 = sign_field(w->gx, 10) + side * sign_field(w->gp, 10);
    int y0 = w->gy;
    int left = x0 > 0 ? x0 : 0, top = y0 > 0 ? y0 : 0;
    int right = x0 + w->w + 1, bottom = y0 + w->h + 1;
    if (right > WIDTH) right = WIDTH;
    if (bottom > HEIGHT) bottom = HEIGHT;
    if (left >= right || top >= bottom) return;
    unsigned maps_x = 1u << w->scx, maps_y = 1u << w->scy;
    unsigned base = w->bg_map_base;
    int source_x = sign_field(w->mx, 13) + side * sign_field(w->mp, 15);
    int source_y = sign_field(w->my, 13);
    if (w->bgm == 0) {
        draw_normal(picture, eye, layer, w, left, top, right, bottom,
                    source_x + left - x0, source_y + top - y0);
        return;
    }
    unsigned params = BGMAP_OFFSET + 2u * w->param;

    for (int y = top; y < bottom; y++) {
        int row = y - y0;
        int dx = 512, dy = 0;
        int u = source_x * 512, v = (source_y + row) * 512;
        if (w->bgm == 1) {
            unsigned eye_offset = eye && !(w->param & 1);
            u += sign_field(word(params + 4 * row + 2 * eye_offset), 13) * 512;
        } else if (w->bgm == 2) {
            unsigned entry = params + 16 * row;
            int mp = (s16)word(entry + 2);
            unsigned offset = (mp < 0 && !eye) || (mp > 0 && eye) ? abs(mp) : 0;
            dx = (s16)word(entry + 6);
            dy = (s16)word(entry + 8);
            u = (s16)word(entry) * 64 + (int)offset * dx;
            v = (s16)word(entry + 4) * 64 + (int)offset * dy;
        }
        u += (left - x0) * dx;
        v += (left - x0) * dy;
        u32 *out = picture->pixel[eye][y];
        unsigned last = NO_CELL, attr = 0;
        const ColorTile *tile = NULL;
        for (int x = left; x < right;) {
            int sx = u >> 9, sy = v >> 9;
            unsigned cell = map_cell(base, maps_x, maps_y, w->is_over, w->over & 2047, sx, sy);
            if (cell != last) {
                last = cell;
                attr = word(BGMAP_OFFSET + 2 * cell);
                tile = resolve(attr, cell, layer, false, eye, x, y & ~7);
            }
            unsigned mask_x = attr & 0x2000 ? 7 : 0;
            unsigned mask_y = attr & 0x1000 ? 7 : 0;
            int length = dx == 512 && dy == 0 ? 8 - (sx & 7) : 1;
            if (length > right - x) length = right - x;
            if (length > 1) {
                const u32 *pixels = tile->colors + (((sy & 7) ^ mask_y) * 8);
                for (int i = 0; i < length; i++)
                    overlay(out + x + i, pixels[((sx + i) & 7) ^ mask_x]);
            } else {
                overlay(out + x, tile->colors[(((sy & 7) ^ mask_y) * 8) + ((sx & 7) ^ mask_x)]);
            }
            u += dx * length;
            v += dy * length;
            x += length;
        }
    }
}

static void draw_sprites(Picture *picture, unsigned eye, unsigned layer, unsigned group) {
    unsigned start = vb_state->tVIPREG.SPT[group] & 1023;
    unsigned end = group ? vb_state->tVIPREG.SPT[group - 1] & 1023 : 1023;
    for (unsigned i = start; i != end; i = (i - 1) & 1023) {
        unsigned obj = OBJ_OFFSET + i * OBJ_SIZE;
        unsigned flags = word(obj + 2);
        if (!(flags & (0x8000 >> eye))) continue;
        int x0 = sign_field(word(obj), 10) + (eye ? 1 : -1) * sign_field(flags, 10);
        int y0 = ram[obj + 4];
        if (y0 > HEIGHT) y0 -= 256;
        if (x0 <= -8 || x0 >= WIDTH || y0 <= -8 || y0 >= HEIGHT) continue;
        unsigned attr = word(obj + 6);
        const ColorTile *tile = resolve(attr, NO_CELL, layer, true, eye, x0, y0);
        for (int y = 0; y < 8; y++) {
            if ((unsigned)(y0 + y) >= HEIGHT) continue;
            unsigned row = (y ^ ((attr & 0x1000) ? 7 : 0)) * 8;
            for (int x = 0; x < 8; x++) {
                if ((unsigned)(x0 + x) >= WIDTH) continue;
                unsigned col = x ^ ((attr & 0x2000) ? 7 : 0);
                overlay(&picture->pixel[eye][y0 + y][x0 + x], tile->colors[row + col]);
            }
        }
    }
}

void rvc_render(unsigned buffer) {
    if (!rvc_active() || !buffers()) {
        if (pictures) pictures[buffer].valid_eyes = 0;
        return;
    }
    Picture *picture = &pictures[buffer];
    ram = (const u8 *)vb_state->V810_DISPLAY_RAM.off;
    unsigned eyes = eye_count == 2 ? 3 : 1u << tVBOpt.DEFAULT_EYE;
    memset(chars, 0, sizeof(chars));
    marker_count[recent ^ 1] = 0;
    if (++epoch == 0) {
        memset(tiles, 0, sizeof(tiles));
        epoch = 1;
    }
    const WORLD *worlds = (const WORLD *)(ram + WORLD_OFFSET);
    for (unsigned eye = 0; eye < 2; eye++) {
        if (!(eyes & (1u << eye))) continue;
        u32 background = COVERED | ((u32)(vb_state->tVIPREG.BKCOL & 3) << SHADE_SHIFT) | 0x7b9fa8;
        for (unsigned y = 0; y < HEIGHT; y++)
            for (unsigned x = 0; x < WIDTH; x++)
                picture->pixel[eye][y][x] = background & ~COVERED;
        unsigned group = 3;
        for (int layer = 31; layer >= 0; layer--) {
            const WORLD *world = &worlds[layer];
            if (world->end) break;
            if (!world->on) continue;
            if (world->on & (2u >> eye)) {
                if (world->bgm == 3) draw_sprites(picture, eye, layer, group);
                else draw_background(picture, eye, layer, world);
            }
            if (world->bgm == 3) group = (group - 1) & 3;
        }
    }
    picture->valid_eyes = eyes;
    recent ^= 1;
}

static inline unsigned capped(unsigned intensity) {
    return intensity > 255 ? 255 : intensity;
}

static inline unsigned channel(unsigned component, unsigned factor) {
    return capped((component * factor + 128) >> 8);
}

C3D_Tex *rvc_present(unsigned vip_buffer, unsigned direct_buffer, unsigned eyes) {
    if (!rvc_active() || !buffers()) return NULL;
    Picture *picture = &pictures[vip_buffer];
    ram = (const u8 *)vb_state->V810_DISPLAY_RAM.off;
    const u8 *column_table = ram + COLTABLE_OFFSET + 1;
    bool vip_visible = vb_state->tVIPREG.XPCTRL & XPEN;
    bool direct_visible = tDSPCACHE.DDSPDataState[direct_buffer] != GPU_CLEAR;
    if (vip_visible && (eyes & picture->valid_eyes) != eyes) return NULL;
    u32 *dest = display.data;
    static const unsigned spread[] = {0, 1, 4, 5, 16, 17, 20, 21};
    const unsigned level[] = {0, vb_state->tVIPREG.BRTA, vb_state->tVIPREG.BRTB,
        vb_state->tVIPREG.BRTA + vb_state->tVIPREG.BRTB + vb_state->tVIPREG.BRTC};

    for (unsigned eye = 0; eye < 2; eye++) {
        if (!(eyes & (1u << eye))) continue;
        const u8 *direct = ram + 0x8000 * direct_buffer + 0x10000 * eye;
        for (unsigned x = 0; x < WIDTH; x += 8) {
            unsigned shade_scale[2][4], paint_scale[2];
            for (unsigned half = 0; half < 2; half++) {
                unsigned repeat = 1 + column_table[eye * 512 + (255 - (x / 4 + half)) * 2];
                unsigned peak = capped((level[3] * repeat * 255 + 64) / 128);
                paint_scale[half] = fade[peak >> 2];
                for (unsigned shade = 0; shade < 4; shade++)
                    shade_scale[half][shade] = capped(level[shade] * repeat * 4);
            }
            unsigned col_base = ((511 - x) >> 3) * 4096 + eye * 2048;
            for (unsigned y = 0; y < HEIGHT; y += 8) {
                u32 *block = dest + col_base + y * 8;
                for (unsigned yy = 0; yy < 8; yy++) {
                    for (unsigned xx = 0; xx < 8; xx++) {
                        u32 pixel = vip_visible ? picture->pixel[eye][y + yy][x + xx] : 0;
                        unsigned shade = (pixel >> SHADE_SHIFT) & 3;
                        unsigned layer = direct_visible ? (direct[(x + xx) * 64 + (y + yy) / 4] >> (2 * (yy & 3))) & 3 : 0;
                        if (layer && (!(pixel & COVERED) || (!tVBOpt.VIP_OVER_SOFT &&
                            (tVBOpt.RENDERMODE == RM_TOGPU || layer != shade)))) {
                            shade = layer;
                            pixel = 0xa2c7ff;
                        }
                        unsigned brightness = pixel & PAINTED ? paint_scale[xx / 4] : shade_scale[xx / 4][shade];
                        u32 rgb = (channel((pixel >> 16) & 255, brightness) << 24) |
                                  (channel((pixel >> 8) & 255, brightness) << 16) |
                                  (channel(pixel & 255, brightness) << 8) | 255;
                        block[(spread[7 - xx] << 1) | spread[yy]] = rgb;
                    }
                }
            }
        }
    }
    C3D_TexFlush(&display);
    return &display;
}
