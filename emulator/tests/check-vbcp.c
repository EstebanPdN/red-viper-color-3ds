// SPDX-License-Identifier: MIT
#include "vbcp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t le32(const uint8_t *p) {
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void check(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size >= 28 && !fseek(file, 0, SEEK_SET));
    uint8_t *bytes = malloc(size);
    assert(bytes && fread(bytes, 1, size, file) == (size_t)size);
    fclose(file);
    uint32_t crc = le32(bytes + size - 8), rom_size = le32(bytes + size - 4);
    Vbcp decoded;
    if (!vbcp_open(&decoded, bytes, size, crc, rom_size)) {
        fprintf(stderr, "Could not decode %s\n", path);
        abort();
    }
    assert(decoded.table[VBCP_TILE].count);
    for (unsigned i = 0; i < decoded.table[VBCP_TILE].count; i++) {
        const uint8_t *original = decoded.table[VBCP_TILE].data + i * 204;
        VbcpPaint paint = vbcp_paint(&decoded, VBCP_TILE, le32(original), 0, 0);
        assert(paint.rgb && paint.rgb == original + 12);
    }
    assert(!vbcp_open(&decoded, bytes, size, crc ^ 1, rom_size));
    assert(!vbcp_open(&decoded, bytes, size, crc, rom_size + 1));
    for (unsigned n = 0; n < 28; n++)
        assert(!vbcp_open(&decoded, bytes, n, crc, rom_size));
    assert(!vbcp_open(&decoded, bytes, size - 1, crc, rom_size));
    memset(bytes + 8, 0xff, 4);
    assert(!vbcp_open(&decoded, bytes, size, crc, rom_size));
    free(bytes);
    printf("PASS %s\n", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
}

int main(int argc, char **argv) {
    const uint8_t tile[16] = {0};
    assert(vbcp_tile_id(tile) == UINT32_C(0x69691905));
    for (int i = 1; i < argc; i++) check(argv[i]);
    return 0;
}
