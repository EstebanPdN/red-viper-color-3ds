// SPDX-License-Identifier: MIT
#ifndef RVC_VIDEO_H
#define RVC_VIDEO_H

#include <citro3d.h>

void rvc_load(const char *rom_path, u32 crc, u32 size);
void rvc_reset(void);
void rvc_quit(void);
bool rvc_active(void);
const char *rvc_pack_status(void);
void rvc_render(unsigned buffer);
C3D_Tex *rvc_present(unsigned vip_buffer, unsigned direct_buffer, unsigned eyes);

#endif
