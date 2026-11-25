// SPDX-License-Identifier: MIT
#ifndef RVC_VBCP_H
#define RVC_VBCP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { VBCP_PIXELS = 64, VBCP_MAX_BYTES = 8 * 1024 * 1024, VBCP_MAX_GROUPS = 256 };
enum { VBCP_TILE, VBCP_LAYER, VBCP_KEEP, VBCP_PALETTE, VBCP_CELL, VBCP_CONTEXT, VBCP_TABLES };

typedef struct {
    const uint8_t *data;
    uint32_t count;
} VbcpTable;

typedef struct {
    VbcpTable table[VBCP_TABLES];
    VbcpTable markers[VBCP_MAX_GROUPS];
    const uint8_t *layer_groups;
    uint32_t group_count, layer_group_bytes;
    uint32_t reference;
} Vbcp;

typedef struct {
    const uint8_t *rgb;
    uint64_t mask, keep;
} VbcpPaint;

// The input stays owned by the caller and must outlive the parsed views.
bool vbcp_open(Vbcp *pack, const uint8_t *bytes, size_t size, uint32_t rom_crc, uint32_t rom_size);
uint32_t vbcp_tile_id(const uint8_t tile[16]);
VbcpPaint vbcp_paint(const Vbcp *pack, unsigned table, uint32_t hash, unsigned detail, unsigned cell);
bool vbcp_has_context(const Vbcp *pack, uint32_t hash);
bool vbcp_marker(const Vbcp *pack, unsigned group, uint32_t hash);
bool vbcp_layer_group(const Vbcp *pack, unsigned group);

#endif
