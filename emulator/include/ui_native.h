#ifndef RVC_UI_NATIVE_H
#define RVC_UI_NATIVE_H
#include <citro2d.h>
#include "ui_assets.h"
bool rvc_ui_init(void);
void rvc_ui_theme(int tint);
void rvc_ui_image(int asset, float x, float y, float w, float h);
void rvc_ui_patch(float x, float y, float w, float h, int state);
void rvc_ui_text(const char *text, float x, float y, bool center);
void rvc_ui_label(const char *text, float x, float y, float w, float h);
void rvc_ui_frame(const char *title);
#endif

void rvc_ui_prepare(void);
