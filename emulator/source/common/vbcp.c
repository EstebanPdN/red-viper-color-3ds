// SPDX-License-Identifier: MIT
#include "vbcp.h"
#include <string.h>

static const unsigned record_size[] = {204, 208, 12, 208, 216, 208};

static uint32_t le(const uint8_t *p, unsigned n) {
    uint32_t value = 0;
    while (n) value = (value << 8) | p[--n];
    return value;
}

static uint64_t mask_at(const uint8_t *p) {
    return le(p, 4) | ((uint64_t)le(p + 4, 4) << 32);
}

static uint64_t key_at(const uint8_t *p, unsigned table) {
    if (table == VBCP_CELL)
        return ((uint64_t)le(p, 2) << 48) | ((uint64_t)le(p + 2, 2) << 32) | le(p + 4, 4);
    if (table == VBCP_LAYER || table == VBCP_PALETTE || table == VBCP_CONTEXT)
        return ((uint64_t)le(p, 4) << 32) | le(p + 4, 4);
    return le(p, 4);
}

static bool records(const uint8_t **next, const uint8_t *end, VbcpTable *result, unsigned width) {
    const uint8_t *p = *next;
    size_t remaining = end - p;
    if (remaining < 4) return false;
    uint32_t n = le(p, 4);
    if (n > (remaining - 4) / width) return false;
    *result = (VbcpTable){p + 4, n};
    *next = p + 4 + (size_t)n * width;
    return true;
}

static bool table(const uint8_t **next, const uint8_t *end, Vbcp *pack, unsigned id) {
    unsigned width = record_size[id];
    VbcpTable *view = &pack->table[id];
    if (!records(next, end, view, width)) return false;
    uint64_t previous = 0;
    for (unsigned i = 0; i < view->count; i++) {
        uint64_t key = key_at(view->data + (size_t)i * width, id);
        if (i && key <= previous) return false;
        previous = key;
    }
    return true;
}

static bool take_tag(const uint8_t **p, const uint8_t *end, const char tag[8]) {
    if (end - *p < 8 || memcmp(*p, tag, 8)) return false;
    *p += 8;
    return true;
}

bool vbcp_open(Vbcp *pack, const uint8_t *data, size_t size, uint32_t crc, uint32_t rom_size) {
    memset(pack, 0, sizeof(*pack));
    if (size < 12 || size > VBCP_MAX_BYTES || memcmp(data, "VBGOCP0", 7)) return false;
    unsigned version = data[7] - '0';
    if (version < 1 || version > 3) return false;
    const uint8_t *p = data + 8, *end = data + size;
    if (size >= 28 && !memcmp(end - 16, "VBGOROM1", 8)) {
        end -= 16;
        if (le(end + 8, 4) != crc || le(end + 12, 4) != rom_size) return false;
    } else if (version == 3) {
        return false;
    }
    Vbcp parsed = {.reference = 63};
    if (!table(&p, end, &parsed, VBCP_TILE)) return false;
    if (version >= 2 && (!table(&p, end, &parsed, VBCP_LAYER) || !table(&p, end, &parsed, VBCP_KEEP)))
        return false;
    if (version == 3) {
        if (!table(&p, end, &parsed, VBCP_PALETTE) || !table(&p, end, &parsed, VBCP_CELL) || end - p < 4)
            return false;
        parsed.reference = le(p, 4);
        if (parsed.reference > 63) return false;
        if (!parsed.reference) parsed.reference = 63;
        p += 4;
        if (take_tag(&p, end, "VBGOCTX1")) {
            if (end - p < 4) return false;
            parsed.group_count = le(p, 4);
            p += 4;
            if (parsed.group_count > VBCP_MAX_GROUPS) return false;
            for (unsigned g = 0; g < parsed.group_count; g++)
                if (!records(&p, end, &parsed.markers[g], 4)) return false;
            if (!table(&p, end, &parsed, VBCP_CONTEXT)) return false;
            if (take_tag(&p, end, "VBGOCTXL")) parsed.layer_group_bytes = 8;
            else if (take_tag(&p, end, "VBGOCTXW")) parsed.layer_group_bytes = 32;
            if ((size_t)(end - p) < parsed.layer_group_bytes) return false;
            parsed.layer_groups = p;
            p += parsed.layer_group_bytes;
        }
        VbcpTable extension;
        if (take_tag(&p, end, "VBGOAMB1") && !records(&p, end, &extension, 4)) return false;
        if (take_tag(&p, end, "VBGOEYT1") && !records(&p, end, &extension, 12)) return false;
    }
    if (p != end) return false;
    *pack = parsed;
    return true;
}

uint32_t vbcp_tile_id(const uint8_t tile[16]) {
    // The wire ID is 32-bit FNV-1a over the sixteen raw little-endian tile bytes.
    uint32_t hash = UINT32_C(0x811c9dc5);
    for (size_t i = 0; i != 16; ++i)
        hash = (hash ^ tile[i]) * UINT32_C(0x01000193);
    return hash;
}

VbcpPaint vbcp_paint(const Vbcp *pack, unsigned id, uint32_t hash, unsigned detail, unsigned cell) {
    VbcpTable view = pack->table[id];
    if (!view.count) return (VbcpPaint){0};
    uint64_t key = hash;
    unsigned offset = 4;
    if (id == VBCP_CELL) {
        key |= ((uint64_t)cell << 48) | ((uint64_t)detail << 32);
        offset = 8;
    } else if (id == VBCP_LAYER || id == VBCP_PALETTE || id == VBCP_CONTEXT) {
        key = ((uint64_t)hash << 32) | detail;
        offset = 8;
    }
    unsigned width = record_size[id];
    uint32_t lo = 0, hi = view.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (key_at(view.data + (size_t)mid * width, id) < key) lo = mid + 1;
        else hi = mid;
    }
    const uint8_t *entry = view.data + (size_t)lo * width;
    if (lo == view.count || key_at(entry, id) != key) return (VbcpPaint){0};
    if (id == VBCP_KEEP) return (VbcpPaint){.keep = mask_at(entry + offset)};
    uint64_t keep = id == VBCP_CELL ? mask_at(entry + offset + 8) : 0;
    return (VbcpPaint){entry + offset + (id == VBCP_CELL ? 16 : 8), mask_at(entry + offset), keep};
}

bool vbcp_has_context(const Vbcp *pack, uint32_t hash) {
    VbcpTable view = pack->table[VBCP_CONTEXT];
    uint32_t lo = 0, hi = view.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (le(view.data + (size_t)mid * 208, 4) < hash) lo = mid + 1;
        else hi = mid;
    }
    return lo < view.count && le(view.data + (size_t)lo * 208, 4) == hash;
}

bool vbcp_marker(const Vbcp *pack, unsigned group, uint32_t hash) {
    VbcpTable view = pack->markers[group];
    for (unsigned i = 0; i < view.count; i++)
        if (le(view.data + (size_t)i * 4, 4) == hash) return true;
    return false;
}

bool vbcp_layer_group(const Vbcp *pack, unsigned group) {
    return group / 8 < pack->layer_group_bytes && ((pack->layer_groups[group / 8] >> (group % 8)) & 1);
}
