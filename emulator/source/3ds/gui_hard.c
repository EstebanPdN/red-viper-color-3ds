#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <malloc.h>
#include <arpa/inet.h>
#include <zlib.h>
#include "c2d/sprite.h"
#include "c2d/text.h"
#include <citro2d.h>
#include "drc_core.h"
#include "vb_dsp.h"
#include "vb_gui.h"
#include "v810_mem.h"
#include "vb_set.h"
#include "rvc_video.h"
#include "ui_native.h"
#include "vb_sound.h"
#include "replay.h"
#include "main.h"
#include "periodic.h"
#include "sprites_t3x.h"
#include "sprites.h"
#include "splash_t3x.h"
#include "splash.h"
#include "vblink.h"
#include "cpp.h"
#include "extrapad.h"
#include "multiplayer.h"
#include "rom_db.h"
#include "patches.h"
#include "video_hard.h"

#define COLOR_R(COLOR) ( ((COLOR) & 0x000000FF) )
#define COLOR_G(COLOR) ( ((COLOR) & 0x0000FF00) >> 8)
#define COLOR_B(COLOR) ( ((COLOR) & 0x00FF0000) >> 16 )
#define COLOR_BRIGHTNESS(COLOR, BRIGHTNESS) ( C2D_Color32(COLOR_R(COLOR) * ( BRIGHTNESS ), COLOR_G(COLOR) * ( BRIGHTNESS ), COLOR_B(COLOR) * ( BRIGHTNESS ), 255) )

#define TINT_R ( COLOR_R(tVBOpt.TINT) )
#define TINT_G ( COLOR_G(tVBOpt.TINT) )
#define TINT_B ( COLOR_B(tVBOpt.TINT) )

#define BLACK ( C2D_Color32(0, 0, 0, 255) )
#define TINT_BRIGHTNESS(BRIGHTNESS) ( COLOR_BRIGHTNESS(tVBOpt.TINT, BRIGHTNESS) )
#define TINT_100 TINT_BRIGHTNESS(1.0)
#define TINT_90 TINT_BRIGHTNESS(0.9)
#define TINT_75 TINT_BRIGHTNESS(0.75)
#define TINT_50 TINT_BRIGHTNESS(0.5)
#define TINT_COLOR TINT_100

#define PERFORM_FOR_EACH_3DS_BUTTON(MACRO) \
    MACRO(DUP) \
    MACRO(DDOWN) \
    MACRO(DLEFT) \
    MACRO(DRIGHT) \
    MACRO(CPAD_UP) \
    MACRO(CPAD_DOWN) \
    MACRO(CPAD_LEFT) \
    MACRO(CPAD_RIGHT) \
    MACRO(CSTICK_UP) \
    MACRO(CSTICK_DOWN) \
    MACRO(CSTICK_LEFT) \
    MACRO(CSTICK_RIGHT) \
    MACRO(A) \
    MACRO(X) \
    MACRO(B) \
    MACRO(Y) \
    MACRO(START) \
    MACRO(SELECT) \
    MACRO(L) \
    MACRO(R) \
    MACRO(ZL) \
    MACRO(ZR)

#define PERFORM_FOR_EACH_VB_BUTTON(MACRO) \
    MACRO(KEY_L) \
    MACRO(KEY_R) \
    MACRO(KEY_SELECT) \
    MACRO(KEY_START) \
    MACRO(KEY_B) \
    MACRO(KEY_A) \
    MACRO(RPAD_R) \
    MACRO(RPAD_L) \
    MACRO(RPAD_D) \
    MACRO(RPAD_U) \
    MACRO(LPAD_R) \
    MACRO(LPAD_L) \
    MACRO(LPAD_D) \
    MACRO(LPAD_U)

bool buttons_on_screen = false;
bool guiShouldSwitch(void);
void drawTouchControls(int inputs);

bool old_2ds = false;
bool any_2ds = false;

extern C3D_RenderTarget *finalScreen[2];

static C3D_RenderTarget *screen;

static C2D_TextBuf static_textbuf;
static C2D_TextBuf dynamic_textbuf;

static C2D_Text text_A, text_B, text_btn_A, text_btn_B, text_btn_X, text_btn_L, text_btn_R,
                text_switch, text_saving, text_on, text_off, text_toggle, text_hold, text_nintendo_3ds,
                text_vbipd, text_left, text_right, text_sound_error, text_anykeyexit, text_about,
                text_debug_filenames, text_loading, text_loaderr, text_unloaded, text_yes, text_no,
                text_areyousure_reset, text_areyousure_exit, text_savestate_menu, text_save, text_load,
                text_vb_lpad, text_vb_rpad, text_mirror_abxy, text_vblink, text_preset, text_custom,
                text_error, text_3ds, text_vb, text_map, text_currently_mapped_to, text_normal, text_turbo,
                text_current_default, text_anaglyph, text_depth, text_cpp_on, text_cpp_off,
                text_monochrome, text_multicolor, text_brighten, text_brightness_disclaimer,
                text_multi_waiting, text_multi_init_error, text_multi_disconnect, text_multi_comm_error,
                text_multi_reset_on_join, text_input_buffer, text_areyousure_leave,
                text_version_mismatch, text_your_version, text_connect_anyway,
                text_multi_no_match, text_multi_not_loaded, text_dlplay_saving, text_multi_preparing,
                text_downloading, text_forwarder_nomatch, text_protocol_mismatch;

#define CUSTOM_3DS_BUTTON_TEXT(BUTTON) static C2D_Text text_custom_3ds_button_##BUTTON;
PERFORM_FOR_EACH_3DS_BUTTON(CUSTOM_3DS_BUTTON_TEXT)
#define CUSTOM_VB_BUTTON_TEXT(BUTTON) static C2D_Text text_custom_vb_button_##BUTTON;
PERFORM_FOR_EACH_VB_BUTTON(CUSTOM_VB_BUTTON_TEXT)

static C2D_SpriteSheet sprite_sheet;
static C2D_SpriteSheet splash_sheet;
static C2D_Sprite colour_wheel_sprite, logo_sprite, vb_icon_sprite;
static C2D_Sprite text_3ds_sprite, text_vb_sprite, text_toggle_sprite, text_turbo_sprite;
static C2D_Sprite splash_left, splash_right;

// helpers
#define dis(X1,Y1,X2,Y2) ( (((X2)-(X1)) * ((X2)-(X1))) + (((Y2)-(Y1)) * ((Y2)-(Y1))) )

static inline int sqr(int i) {
    return i * i;
}

static inline int u16len(const u16 *s) {
    const u16 *e = s;
    while (*e) e++;
    return e - s;
}

typedef struct Button_t Button;
struct Button_t {
    char *str;
    float x, y, w, h;
    bool show_toggle, toggle, show_option, hidden, draw_selected_rect, disabled, secondary;
    int option;
    int icon_plus_one;
    int colour;
    C2D_Text *toggle_text_on;
    C2D_Text *toggle_text_off;
    C2D_Text **option_texts;
    C2D_Text text;
    void (*custom_draw)(Button*);
};

static Button* selectedButton = NULL;
static bool buttonLock = false;

static inline int handle_buttons(Button buttons[], int count);
#define HANDLE_BUTTONS(buttons) handle_buttons((buttons), sizeof((buttons)) / sizeof((buttons)[0]))

#define STATIC_TEXT(var, str) {\
    C2D_TextParse(var, static_textbuf, str); \
    C2D_TextOptimize(var); \
}

#define SETUP_BUTTONS(arr) \
    for (int i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) { \
        if (arr[i].str) { \
            STATIC_TEXT(&arr[i].text, arr[i].str ? arr[i].str : ""); \
        } \
    }

#define LOOP_BEGIN(buttons, initial_button) \
    if (initial_button >= 0) selectedButton = &(buttons)[initial_button]; \
    int button = -1; \
    bool loop = true; \
    while (loop && aptMainLoop()) { \
        rvc_ui_theme(tVBOpt.TINT); \
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW); \
        C2D_TargetClear(screen, 0); \
        video_flush(true); \
        C2D_SceneBegin(screen); \
        rvc_ui_prepare(); \
        hidScanInput(); \

#define DEFAULT_RETURN

#define LOOP_END(buttons) \
        if (loop) button = HANDLE_BUTTONS((buttons)); \
        if (button >= 0) loop = false; \
        C2D_Flush(); \
        C3D_FrameEnd(0); \
    } \
    if (loop) { \
        /* home menu exit */ \
        guiop = GUIEXIT; \
        return DEFAULT_RETURN; \
    }

static Button no_buttons[] = {{.hidden = true}};

static void main_menu(int initial_button);

static void first_menu(int initial_button);
static Button first_menu_buttons[] = {
    {.str="Load ROM", .x=16, .y=16, .w=288, .h=144},
    {.str="Multi\nplayer", .x=0, .y=176, .w=80, .h=64},
    {.str="Options", .x=240, .y=176, .w=80, .h=64},
    {.str="Quit", .x=112, .y=192, .w=96, .h=48},
};

static void game_menu(int initial_button);
static Button game_menu_buttons[] = {
    #define MAIN_MENU_LOAD_ROM 0
    {.str="Load ROM", .x=232 - 16, .y=64, .w=80 + 16, .h=80},
    #define MAIN_MENU_MULTI 1
    {.str="Multi\nplayer", .x=0, .y=176, .w=80, .h=64},
    #define MAIN_MENU_OPTIONS 2
    {.str="Options", .x=240, .y=176, .w=80, .h=64},
    #define MAIN_MENU_QUIT 3
    {.str="Quit", .x=112, .y=192, .w=96, .h=48},
    #define MAIN_MENU_RESUME 4
    {.str="Resume", .x=0, .y=0, .w=320, .h=48},
    #define MAIN_MENU_RESET 5
    {.str="Reset", .x=8, .y=64, .w=80 + 16, .h=80},
    #define MAIN_MENU_SAVESTATES 6
    {.str="Savestates", .x=112, .y=64, .w=80 + 16, .h=80},
};

static void multiplayer_menu(int initial_button);
static Button multiplayer_menu_buttons[] = {
    #define MULTI_MENU_RESUME 0
    {.str="Resume", .x=0, .y=0, .w=320, .h=48},
    #define MULTI_MENU_BUFFER_MINUS 1
    {.str="-", .x=180, .y=100, .w=32, .h=32},
    #define MULTI_MENU_BUFFER_PLUS 2
    {.str="+", .x=180+64, .y=100, .w=32, .h=32},
    #define MULTI_MENU_RESET 3
    {.str="Reset", .x=0, .y=176, .w=80, .h=64},
    #define MULTI_MENU_LEAVE 4
    {.str="Leave", .x=112, .y=192, .w=96, .h=48},
    #define MULTI_MENU_OPTIONS 5
    {.str="Options", .x=240, .y=176, .w=80, .h=64},
};

static void multiplayer_wait_for_peer(void);

static bool rom_loader(char *message);
static Button rom_loader_buttons[] = {
    #define ROM_LOADER_UP 0
    {.str="Up", .x=0, .y=0, .w=32, .h=32},
    #define ROM_LOADER_BACK 1
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void multiplayer_main(int initial_button, bool init_uds);
static Button multiplayer_main_buttons[] = {
    #define MULTI_MAIN_HOST 0
    {.str="Host", .x=16, .y=16, .w=288, .h=48},
    #define MULTI_MAIN_JOIN 1
    {.str="Join", .x=16, .y=80, .w=288, .h=48},
    #define MULTI_MAIN_BACK 2
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void multiplayer_join(void);
static Button multiplayer_join_buttons[] = {
    {.str="", .x=320/2-224/2, .y=4+50*0, .w=224, .h=48},
    {.str="", .x=320/2-224/2, .y=4+50*1, .w=224, .h=48},
    {.str="", .x=320/2-224/2, .y=4+50*2, .w=224, .h=48},
    {.str="", .x=320/2-224/2, .y=4+50*3, .w=224, .h=48},
    #define MULTI_JOIN_COUNT 4
    #define MULTI_JOIN_REFRESH 4
    {.str="Refresh", .x=320-68, .y=208, .w=68, .h=32},
    #define MULTI_JOIN_BACK 5
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void multiplayer_host(void);
static Button multiplayer_host_buttons[] = {
    #define MULTI_HOST_LEAVE 0
    {.str = "Cancel", .x=0, .y=208, .w=64, .h=32},
    #define MULTI_HOST_CHANGE 1
    {.str = "Change ROM", .x=160-64, .y=180, .w=64*2, .h=48},
};

static void multiplayer_prepare_join(int initial_button);
static Button multiplayer_prepare_join_buttons[] = {
    #define MULTI_PREPARE_SD 0
    {.str="Load from\nSD Card", .x=32, .y=140, .w=112, .h=52},
    #define MULTI_PREPARE_DLPLAY 1
    {.str="Download\nPlay", .x=176, .y=140, .w=112, .h=52},
    #define MULTI_PREPARE_LEAVE 2
    {.str="Leave", .x=0, .y=208, .w=56, .h=32},
};

static void multiplayer_dlplay(void);
static Button multiplayer_dlplay_buttons[] = {
    #define MULTI_ROOM_LEAVE 0
    {.str = "Cancel", .x=160-48, .y=180, .w=48*2, .h=48},
};

static void multiplayer_sram_transfer(void);

static void multiplayer_ready_room(bool is_host, int initial_button);
static Button multiplayer_ready_room_buttons[] = {
    #define MULTI_READY_START 0
    {.str = "Start", .x=160-48, .y=180, .w=48*2, .h=48},
    #define MULTI_READY_BUFFER_MINUS 1
    {.str="-", .x=180, .y=140, .w=32, .h=32},
    #define MULTI_READY_BUFFER_PLUS 2
    {.str="+", .x=180+64, .y=140, .w=32, .h=32},
    #define MULTI_READY_LEAVE 3
    {.str="Leave", .x=0, .y=208, .w=56, .h=32},
};

static void multiplayer_error(int err, C2D_Text *messsage, bool exit_to_menu);
static Button multiplayer_error_buttons[] = {
    {.str = "Exit", .x=160-48, .y=180, .w=48*2, .h=48},
};

static void controls(int initial_button);
static Button controls_buttons[] = {
    #define CONTROLS_CONTROL_SCHEME 0
    {.str="Control Scheme", .x=60, .y=16, .w=200, .h=48, .show_toggle=true, .toggle_text_on=&text_custom, .toggle_text_off=&text_preset},
    #define CONTROLS_CONFIGURE_SCHEME 1
    {.str="Configure Scheme", .x=60, .y=80, .w=200, .h=48},
    #define CONTROLS_TOUCHSCREEN 2
    {.str="Touchscreen settings", .x=60, .y=144, .w=200, .h=48},
    #define CONTROLS_DISPLAY 3
    {.str="Input display", .x=200, .y=200, .w=120, .h=40, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
    #define CONTROLS_CPP 4
    {.str="Circle Pad Pro", .x=60, .y=200, .w=130, .h=40},
    #define CONTROLS_BACK 5
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void cpp_options(int initial_button);
static Button cpp_options_buttons[] = {
    #define CPP_TOGGLE 0
    {.str = "CPP enabled", .x=60, .y=16, .w=200, .h=48, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
    #define CPP_CALIBRATE 1
    {.str="Calibrate CPP", .x=60, .y=80, .w=200, .h=48},
    #define CPP_BACK 2
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void draw_abxy(Button*);
static void draw_shoulders(Button*);
static void preset_controls(int initial_button);
static Button preset_controls_buttons[] = {
    #define PRESET_CONTROLS_FACE 0
    // y is set later
    {.x=160-64, .w=128, .h=80, .custom_draw=draw_abxy},
    #define PRESET_CONTROLS_SHOULDER 1
    {.x=160-64, .y=0, .w=128, .h=40, .custom_draw=draw_shoulders},
    #define PRESET_CONTROLS_DPAD_MODE 2
    {.str="3DS D-Pad Mode", .x=60, .y=144, .w=200, .h=40, .show_option=true, .option_texts=(C2D_Text*[]){&text_vb_lpad, &text_vb_rpad, &text_mirror_abxy}},
    #define PRESET_CONTROLS_BACK 3
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

#define DECLARE_DRAW_CUSTOM_3DS_BUTTON_FUNCTION(CUSTOM_3DS_BUTTON) static void draw_custom_3ds_##CUSTOM_3DS_BUTTON(Button*);
PERFORM_FOR_EACH_3DS_BUTTON(DECLARE_DRAW_CUSTOM_3DS_BUTTON_FUNCTION)

static void custom_3ds_mappings(int initial_button);
static Button custom_3ds_mappings_buttons[] = {
    #define CUSTOM_3DS_MAPPINGS_L 0
    {.x=0, .y=0, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_L},
    #define CUSTOM_3DS_MAPPINGS_ZL 1
    {.x=108, .y=0, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_ZL},
    #define CUSTOM_3DS_MAPPINGS_ZR 2
    {.x=162, .y=0, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_ZR},
    #define CUSTOM_3DS_MAPPINGS_R 3
    {.x=270, .y=0, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_R},
    #define CUSTOM_3DS_MAPPINGS_CPAD_UP 4
    {.x=54, .y=30, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CPAD_UP},
    #define CUSTOM_3DS_MAPPINGS_CPAD_DOWN 5
    {.x=54, .y=76, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CPAD_DOWN},
    #define CUSTOM_3DS_MAPPINGS_CPAD_LEFT 6
    {.x=0, .y=53, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CPAD_LEFT},
    #define CUSTOM_3DS_MAPPINGS_CPAD_RIGHT 7
    {.x=108, .y=53, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CPAD_RIGHT},
    #define CUSTOM_3DS_MAPPINGS_DUP 8
    {.x=54, .y=122, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_DUP},
    #define CUSTOM_3DS_MAPPINGS_DDOWN 9
    {.x=54, .y=168, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_DDOWN},
    #define CUSTOM_3DS_MAPPINGS_DLEFT 10
    {.x=0, .y=145, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_DLEFT},
    #define CUSTOM_3DS_MAPPINGS_DRIGHT 11
    {.x=108, .y=145, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_DRIGHT},
    #define CUSTOM_3DS_MAPPINGS_CSTICK_UP 12
    {.x=216, .y=30, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CSTICK_UP},
    #define CUSTOM_3DS_MAPPINGS_CSTICK_DOWN 13
    {.x=216, .y=76, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CSTICK_DOWN},
    #define CUSTOM_3DS_MAPPINGS_CSTICK_LEFT 14
    {.x=162, .y=53, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CSTICK_LEFT},
    #define CUSTOM_3DS_MAPPINGS_CSTICK_RIGHT 15
    {.x=270, .y=53, .w=50, .h=42, .draw_selected_rect=true, .colour=0x808080, .custom_draw=draw_custom_3ds_CSTICK_RIGHT},
    #define CUSTOM_3DS_MAPPINGS_X 16
    {.x=216, .y=122, .w=50, .h=42, .draw_selected_rect=true, .colour=0xFF8000, .custom_draw=draw_custom_3ds_X},
    #define CUSTOM_3DS_MAPPINGS_B 17
    {.x=216, .y=168, .w=50, .h=42, .draw_selected_rect=true, .colour=0x00FFFF, .custom_draw=draw_custom_3ds_B},
    #define CUSTOM_3DS_MAPPINGS_Y 18
    {.x=162, .y=145, .w=50, .h=42, .draw_selected_rect=true, .colour=0x00FF00, .custom_draw=draw_custom_3ds_Y},
    #define CUSTOM_3DS_MAPPINGS_A 19
    {.x=270, .y=145, .w=50, .h=42, .draw_selected_rect=true, .colour=0x0000FF, .custom_draw=draw_custom_3ds_A},
    #define CUSTOM_3DS_MAPPINGS_SELECT 20
    {.x=108, .y=198, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_SELECT},
    #define CUSTOM_3DS_MAPPINGS_START 21
    {.x=162, .y=198, .w=50, .h=42, .draw_selected_rect=true, .custom_draw=draw_custom_3ds_START},
    #define CUSTOM_3DS_MAPPINGS_RESET 22
    {.str="Reset", .x=268, .y=208, .w=52, .h=32},
    #define CUSTOM_3DS_MAPPINGS_BACK 23
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void custom_vb_mappings(int initial_button);
static Button custom_vb_mappings_buttons[] = {
    #define CUSTOM_VB_MAPPINGS_KEY_L 0
    {.str="\uE004", .x=0, .y=0, .w=64, .h=38, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_KEY_R 1
    {.str="\uE005", .x=256, .y=0, .w=64, .h=38, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_LPAD_U 2
    {.str="L\uE079", .x=34, .y=42, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_LPAD_D 3
    {.str="L\uE07A", .x=34, .y=122, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_LPAD_L 4
    {.str="L\uE07B", .x=2, .y=82, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_LPAD_R 5
    {.str="L\uE07C", .x=66, .y=82, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_RPAD_U 6
    {.str="R\uE079", .x=226, .y=42, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_RPAD_D 7
    {.str="R\uE07A", .x=226, .y=122, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_RPAD_L 8
    {.str="R\uE07B", .x=194, .y=82, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_RPAD_R 9
    {.str="R\uE07C", .x=258, .y=82, .w=60, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_KEY_SELECT 10
    {.str="SELECT", .x=58, .y=162, .w=76, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_KEY_START 11
    {.str="START", .x=82, .y=202, .w=76, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_KEY_B 12
    {.str="\uE001", .x=162, .y=202, .w=76, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_KEY_A 13
    {.str="\uE000", .x=186, .y=162, .w=76, .h=36, .draw_selected_rect=true},
    #define CUSTOM_VB_MAPPINGS_BACK 14
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
    #define CUSTOM_VB_MAPPINGS_MOD 15
    {.str="Mod", .x=260, .y=200, .w=60, .h=40, .show_option=true, .option_texts=(C2D_Text*[]){&text_normal, &text_toggle, &text_turbo}},
};

static void touchscreen_settings(void);
static Button touchscreen_settings_buttons[] = {
    #define TOUCHSCREEN_BACK 0
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
    #define TOUCHSCREEN_RESET 1
    {.str="Reset", .x=0, .y=0, .w=52, .h=24},
    #define TOUCHSCREEN_SWITCH 2
    {.str="Toggle Switch", .x=0, .y=28, .w=128, .h=24},
    #define TOUCHSCREEN_DEFAULT 3
    {.str="Make default", .x=0, .y=56, .w=110, .h=24},
};

static void options(int initial_button);
static Button options_buttons[] = {
    #define OPTIONS_VIDEO 0
    {.str="Video\nsettings", .x=16, .y=16, .w=96-8, .h=48},
    #define OPTIONS_CONTROLS 1
    {.str="Controls", .x=112-2, .y=16, .w=96+4, .h=48},
    #define OPTIONS_SOUND 2
    {.str="Sound", .x=208+8, .y=16, .w=96-8, .h=48, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
    #define OPTIONS_PERF 3
    {.str="Perf.\nsettings", .x=16, .y=80, .w=96-8, .h=48},
    #define OPTIONS_FF 4
    {.str="Fastforward", .x=112-2, .y=80, .w=96+4, .h=48, .show_toggle=true, .toggle_text_on=&text_toggle, .toggle_text_off=&text_hold},
    #define OPTIONS_ABOUT 5
    {.str="About", .x=208+8, .y=80, .w=96-8, .h=48},
    #define OPTIONS_SAVE_GLOBAL 6
    {.str="Save\n(Global)", .x=16, .y=144, .w=96-8, .h=48},
    #define OPTIONS_SAVE_GAME 7
    {.str="Save\n(Game)", .x=112-2, .y=144, .w=96+4, .h=48},
    #define OPTIONS_DISCARD 8
    {.str="Discard", .x=208+8, .y=144, .w=96-8, .h=48},
    #define OPTIONS_RESET_TO_GLOBAL 9
    {.str="Restore\nGlobal", .y=144, .h=48},
    #define OPTIONS_BACK  10
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
    #define OPTIONS_DEBUG 11
    {.str="Save debug info", .x=170, .y=208, .w=150, .h=32},
};

static void video_settings(int initial_button);
static Button video_settings_buttons[] = {
    #define VIDEO_MODE 0
    {.str="3D mode", .x=16, .y=16, .w=128, .h=48, .show_toggle=true, .toggle_text_on=&text_anaglyph, .toggle_text_off=&text_nintendo_3ds},
    #define VIDEO_SETTINGS 1
    {.str="Settings", .x=176, .y=16, .w=128, .h=48},
    #define VIDEO_ANTIFLICKER 2
    {.str="Antiflicker", .x=16, .y=80, .w=288, .h=48, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
    #define VIDEO_SLIDER 3
    {.str = "Slider mode", .x=16, .y=80+64, .w=288, .h=48, .show_toggle=true, .toggle_text_on=&text_vbipd, .toggle_text_off=&text_nintendo_3ds},
    #define VIDEO_BACK 4
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void barrier_settings(int initial_button);
static Button barrier_settings_buttons[] = {
    #define BARRIER_MODE 0
    {.str="Color mode", .x=16, .y=16, .w=288, .h=40, .show_toggle=true, .toggle_text_on=&text_multicolor, .toggle_text_off=&text_monochrome},
    #define BARRIER_SETTINGS 1
    {.str="Color settings", .x=16, .y=64, .w=288, .h=40},
    #define BARRIER_DEFAULT_EYE 2
    {.str="Default eye", .x=16, .y=112, .w=288, .h=40, .show_toggle=true, .toggle_text_on=&text_right, .toggle_text_off=&text_left},
    #define BARRIER_BACK 3
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
    #define BARRIER_COLOR_PACK 4
    {.str="Color mode", .x=16, .y=160, .w=288, .h=40, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
};

static void anaglyph_settings(int initial_button);
static Button anaglyph_settings_buttons[] = {
    #define ANAGLYPH_LEFT_OFF 0
    {.x=50, .y=16+24*0, .w=80, .h=20, .colour=0x404040},
    #define ANAGLYPH_LEFT_RED 1
    {.x=50, .y=16+24*1, .w=80, .h=20, .colour=0x0000FF},
    #define ANAGLYPH_LEFT_GREEN 2
    {.x=50, .y=16+24*2, .w=80, .h=20, .colour=0x00FF00},
    #define ANAGLYPH_LEFT_YELLOW 3
    {.x=50, .y=16+24*3, .w=80, .h=20, .colour=0x00FFFF},
    #define ANAGLYPH_LEFT_BLUE 4
    {.x=50, .y=16+24*4, .w=80, .h=20, .colour=0xFF0000},
    #define ANAGLYPH_LEFT_MAGENTA 5
    {.x=50, .y=16+24*5, .w=80, .h=20, .colour=0xFF00FF},
    #define ANAGLYPH_LEFT_CYAN 6
    {.x=50, .y=16+24*6, .w=80, .h=20, .colour=0xFFFF00},
    #define ANAGLYPH_LEFT_WHITE 7
    {.x=50, .y=16+24*7, .w=80, .h=20, .colour=0xFFFFFF},

    #define ANAGLYPH_RIGHT_OFF 8
    {.x=176, .y=16+24*0, .w=80, .h=20, .colour=0x404040},
    #define ANAGLYPH_RIGHT_RED 9
    {.x=176, .y=16+24*1, .w=80, .h=20, .colour=0x0000FF},
    #define ANAGLYPH_RIGHT_GREEN 10
    {.x=176, .y=16+24*2, .w=80, .h=20, .colour=0x00FF00},
    #define ANAGLYPH_RIGHT_YELLOW 11
    {.x=176, .y=16+24*3, .w=80, .h=20, .colour=0x00FFFF},
    #define ANAGLYPH_RIGHT_BLUE 12
    {.x=176, .y=16+24*4, .w=80, .h=20, .colour=0xFF0000},
    #define ANAGLYPH_RIGHT_MAGENTA 13
    {.x=176, .y=16+24*5, .w=80, .h=20, .colour=0xFF00FF},
    #define ANAGLYPH_RIGHT_CYAN 14
    {.x=176, .y=16+24*6, .w=80, .h=20, .colour=0xFFFF00},
    #define ANAGLYPH_RIGHT_WHITE 15
    {.x=176, .y=16+24*7, .w=80, .h=20, .colour=0xFFFFFF},

    #define ANAGLYPH_BACK 16
    {.str="Back", .x=0, .y=208, .w=48, .h=32},

    #define ANAGLYPH_DEPTH_PLACEHOLDER 17
    {.x=240, .y=16, .w=1, .h=1, .hidden=true},
};

static void __attribute__((unused)) colour_filter(void);
static Button colour_filter_buttons[] = {
    #define COLOUR_BACK 0
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
    #define COLOUR_RED 1
    {.str="Red", .x=16, .y=64, .w=48, .h=32},
    #define COLOUR_GRAY 2
    {.str="Gray", .x=16, .y=128, .w=48, .h=32},
};

static void draw_multislot(Button*);
static void multicolour_picker(int initial_button);
static Button multicolour_picker_buttons[] = {
    {.x=16, .y=16, .w=200, .h=40, .custom_draw=draw_multislot},
    {.str="Edit", .x=224, .y=16, .w=80, .h=40},
    {.x=16, .y=16+48, .w=200, .h=40, .custom_draw=draw_multislot},
    {.str="Edit", .x=224, .y=16+48, .w=80, .h=40},
    {.x=16, .y=16+48*2, .w=200, .h=40, .custom_draw=draw_multislot},
    {.str="Edit", .x=224, .y=16+48*2, .w=80, .h=40},
    {.x=16, .y=16+48*3, .w=200, .h=40, .custom_draw=draw_multislot},
    {.str="Edit", .x=224, .y=16+48*3, .w=80, .h=40},
    #define MULTIPICKER_BACK 8
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void multicolour_settings(int palette_id, int initial_button);
static Button multicolour_settings_buttons[] = {
    #define MULTI_BLACK 0
    {.str="Darkest", .x=16, .y=16, .w=170, .h=40},
    #define MULTI_BRTA
    {.str="Dark", .x=16, .y=16+48, .w=170, .h=40},
    #define MULTI_BRTB
    {.str="Light", .x=16, .y=16+48*2, .w=170, .h=40},
    #define MULTI_BRTC
    {.str="Lightest", .x=16, .y=16+48*3, .w=170, .h=40},
    #define MULTI_BACK 4
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static void __attribute__((unused)) multicolour_wheel(int palette_id, int colour_id);
static Button multicolour_wheel_buttons[] = {
    #define MULTIWHEEL_BACK 0
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
    #define MULTIWHEEL_HEX 1
    {.str="", .x=100, .y=212, .w=100, .h=28},
    #define MULTIWHEEL_SCALE 2
    {.str="", .x=220, .y=212, .w=100, .h=28},
};

static void vblink(void);
static Button vblink_buttons[] = {};

static void dev_options(int initial_button);
static Button dev_options_buttons[] = {
    #define PERF_BAR 0
    {.str="Status bar", .x=16, .y=16, .w=288, .h=48, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
    #define PERF_VIP 1
    {.str="Overclock VIP", .x=16, .y=80, .w=288, .h=48, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
    #define PERF_N3DS 2
    {.str="N3DS speedup", .x=16, .y=80+64, .w=288, .h=48, .show_toggle=true, .toggle_text_on=&text_on, .toggle_text_off=&text_off},
    #define PERF_BACK 3
    {.str="Back", .x=0, .y=208, .w=48, .h=32},
};

static bool areyousure(C2D_Text *message);
static Button areyousure_buttons[] = {
    #define AREYOUSURE_YES 0
    {.str="Yes", .x=160-48-32, .y=180, .w=64, .h=48},
    #define AREYOUSURE_NO 1
    {.str="No", .x=160+32, .y=180, .w=64, .h=48},
};

static void savestate_menu(int initial_button, int selected_state);
static Button savestate_buttons[] = {
    #define SAVESTATE_BACK 0
    {.str="Back", .x=0, .y=208, .w=60, .h=32},
    #define SAVE_SAVESTATE 1
    {.str="Save", .x=80, .y=170, .w=70, .h=70},
    #define LOAD_SAVESTATE 2
    {.str="Load", .x=320 - 150, .y=170, .w=70, .h=70},
    #define DELETE_SAVESTATE 3
    {.str="Delete", .x=260, .y=208, .w=60, .h=32},
    #define PREV_SAVESTATE 4
    {.str="<\n\uE004", .x=16, .y=60, .w=40, .h=100},
    #define NEXT_SAVESTATE 5
    {.str=">\n\uE005", .x=320 - 56, .y=60, .w=40, .h=100},
};

static void __attribute__((unused)) savestate_confirm(char *message, int last_button, int selected_state);
static Button savestate_confirm_buttons[] = {
    {.str="Return to game", .x=160-96, .y=140, .w=96*2, .h=48},
    {.str="Return to menu", .x=160-96, .y=190, .w=96*2, .h=48},
};

static void sound_error(void);
static Button sound_error_buttons[] = {
    {.str="Continue without sound", .x=48, .y=130, .w=320-48*2, .h=32},
};

static void about(void);
static Button about_buttons[] = {
    {.str="Back", .x=160-48, .y=180, .w=48*2, .h=48},
};

static bool load_rom(char *rom_message);
static Button load_rom_buttons[] = {
    {.str="Unload & cancel", .x=160-80, .y=180, .w=80*2, .h=48},
};

static void forwarder_error(int err);
static Button forwarder_error_buttons[] = {
    {.str = "Exit", .x=160-48, .y=180, .w=48*2, .h=48},
};

#define SETUP_ALL_BUTTONS \
    SETUP_BUTTONS(game_menu_buttons); \
    SETUP_BUTTONS(multiplayer_menu_buttons); \
    SETUP_BUTTONS(rom_loader_buttons); \
    SETUP_BUTTONS(multiplayer_main_buttons); \
    SETUP_BUTTONS(multiplayer_host_buttons); \
    SETUP_BUTTONS(multiplayer_join_buttons); \
    SETUP_BUTTONS(multiplayer_prepare_join_buttons); \
    SETUP_BUTTONS(multiplayer_dlplay_buttons); \
    SETUP_BUTTONS(multiplayer_ready_room_buttons); \
    SETUP_BUTTONS(multiplayer_error_buttons); \
    SETUP_BUTTONS(controls_buttons); \
    SETUP_BUTTONS(cpp_options_buttons); \
    SETUP_BUTTONS(preset_controls_buttons); \
    SETUP_BUTTONS(custom_3ds_mappings_buttons); \
    SETUP_BUTTONS(custom_vb_mappings_buttons); \
    SETUP_BUTTONS(options_buttons); \
    SETUP_BUTTONS(video_settings_buttons); \
    SETUP_BUTTONS(barrier_settings_buttons); \
    SETUP_BUTTONS(anaglyph_settings_buttons); \
    SETUP_BUTTONS(colour_filter_buttons); \
    SETUP_BUTTONS(multicolour_picker_buttons); \
    SETUP_BUTTONS(multicolour_settings_buttons); \
    SETUP_BUTTONS(multicolour_wheel_buttons); \
    SETUP_BUTTONS(dev_options_buttons); \
    SETUP_BUTTONS(sound_error_buttons); \
    SETUP_BUTTONS(touchscreen_settings_buttons); \
    SETUP_BUTTONS(about_buttons); \
    SETUP_BUTTONS(load_rom_buttons); \
    SETUP_BUTTONS(areyousure_buttons); \
    SETUP_BUTTONS(savestate_buttons); \
    SETUP_BUTTONS(savestate_confirm_buttons); \
    SETUP_BUTTONS(first_menu_buttons);

static int last_savestate = 0;

static void draw_logo(void) {
    rvc_ui_prepare();
    for(int i=0;i<2;i++) {
        C2D_SceneBegin(finalScreen[i]); C2D_ViewReset();
        rvc_ui_image(tVBOpt.FULL_COLOR ? RVC_SPLASH_TOP_FULL : RVC_SPLASH_TOP_ORIGINAL,0,0,400,240);
    }
    C2D_SceneBegin(screen);
}

static bool swkbd_colour(int *save_col);
#include "ui_navigation.inc"

static void first_menu(int initial_button) {
    static bool attempted_forwarder = false;
    if (!attempted_forwarder) {
        attempted_forwarder = true;

        FILE *filename_txt = fopen("romfs:/filename.txt", "r");
        if (!filename_txt) goto no_forwarder;
        char forwarded_path[300] = {0};
        strcpy(forwarded_path, "romfs:/");
        char *filename = forwarded_path + strlen(forwarded_path);
        fread(filename, 1, sizeof(forwarded_path) - 7, filename_txt);
        fclose(filename_txt);
        {
            // trim any newline characters
            char *cr = strchr(filename, '\r');
            if (cr) *cr = 0;
            char *lf = strchr(filename, '\n');
            if (lf) *lf = 0;
        }
        // we have our filename, bail if it doesn't exist
        if (access(forwarded_path, F_OK)) goto no_forwarder;

        strcpy(tVBOpt.ROM_PATH, forwarded_path);
        if (access(tVBOpt.HOME_PATH, F_OK)) mkdir(tVBOpt.HOME_PATH, 0777);
        snprintf(tVBOpt.RAM_PATH, sizeof(tVBOpt.RAM_PATH), "%s/saves/", tVBOpt.HOME_PATH);
        if (access(tVBOpt.RAM_PATH, F_OK)) mkdir(tVBOpt.RAM_PATH, 0777);
        strncat(tVBOpt.RAM_PATH, filename, sizeof(tVBOpt.RAM_PATH) - 1);
        char *extension = strrchr(tVBOpt.RAM_PATH, '.');
        if (!extension) goto no_forwarder;
        strcpy(extension, ".ram");

        // at this point we know we're a forwarder, so just load the rom
        tVBOpt.FORWARDER = true;
        load_rom(NULL);
        return;
    }

    no_forwarder:
    for(;;) {
        const char *labels[]={"Choose ROM","Multiplayer","Settings","About"};
        int b=rvc_list("MENU",labels,4,0,"Exit",NULL);
        if(b<0) return;
        if(b==0) { if(rom_loader(NULL)) return; }
        if(b==1) multiplayer_main(0,true);
        if(b==2) rvc_settings();
        if(b==3) rvc_about();
        if(b==4 && areyousure(&text_areyousure_exit)) { guiop=GUIEXIT; return; }
        if(guiop & GUIEXIT) return;
    }
}

static void game_menu(int initial_button) {
    int focus=0;
    for(;;) {
        const char *labels[]={"Resume","Save / Load","Settings","Session Options"};
        int b=rvc_list("PAUSE",labels,4,focus,"Back",NULL);
        if(b<0) return;
        focus=b;
        if(b==0 || b==4) { guiop=0; return; }
        if(b==1) { savestate_menu(0,last_savestate); }
        if(b==2) rvc_settings();
        if(b==3) {
            const char *session[]={"Change ROM","Restart Game","Multiplayer","Exit Emulator"};
            const char *forward[]={"Restart Game","Multiplayer","Exit Emulator"};
            int c=rvc_list("SESSION OPTIONS",tVBOpt.FORWARDER?forward:session,tVBOpt.FORWARDER?3:4,0,"Back",NULL);
            if(c<0) return;
            if(tVBOpt.FORWARDER) c++;
            if(c==0) { guiop=AKILL|VBRESET; if(rom_loader(NULL)) return; guiop=0; }
            if(c==1 && areyousure(&text_areyousure_reset)) {
                for(int i=0;i<2;i++) { C2D_TargetClear(screenTargetHard[i],0); C2D_TargetClear(finalScreen[i],0); }
                guiop=AKILL|VBRESET; return;
            }
            if(c==2) multiplayer_main(0,true);
            if(c==3 && areyousure(&text_areyousure_exit)) { guiop=AKILL|GUIEXIT; return; }
        }
        if(guiop & GUIEXIT) return;
    }
}

static void main_menu(int initial_button) {
    if (is_multiplayer) {
        [[gnu::musttail]] return multiplayer_menu(0);
    } else {
        my_player_id = 0;
        if (game_running) [[gnu::musttail]] return game_menu(initial_button);
        else [[gnu::musttail]] return first_menu(initial_button);
    }
}

static void multiplayer_menu(int initial_button) {
    C2D_TextBufClear(dynamic_textbuf);
    C2D_Text buffer_text;
    char buffer_str[3] = {0};
    snprintf(buffer_str, sizeof(buffer_str), "%d", tVBOpt.INPUT_BUFFER);
    C2D_TextParse(&buffer_text, dynamic_textbuf, buffer_str);
    C2D_TextOptimize(&buffer_text);

    LOOP_BEGIN(multiplayer_menu_buttons, initial_button);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        if (udsWaitConnectionStatusEvent(false, false)) {
            udsConnectionStatus status;
            udsGetConnectionStatus(&status);
            if (status.total_nodes < 2 || status.status == 11) {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
            }
        }
        C2D_DrawText(&text_input_buffer, C2D_AlignRight | C2D_WithColor, 180 - 8, 104, 0, 0.7, 0.7, TINT_COLOR);
        C2D_DrawText(&buffer_text, C2D_AlignCenter | C2D_WithColor, 180 + 48, 104, 0, 0.7, 0.7, TINT_COLOR);
    LOOP_END(multiplayer_menu_buttons);
    switch (button) {
        case MULTI_MENU_RESUME:
            while (wait_for_free_send_slot(1000000000)) {
                if (udsWaitConnectionStatusEvent(false, false)) {
                    udsConnectionStatus status;
                    udsGetConnectionStatus(&status);
                    if (status.total_nodes < 2 || status.status == 11) {
                        C3D_FrameEnd(0);
                        local_disconnect();
                        udsExit();
                        [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
                    }
                }
            }
            Packet *send_packet = new_packet_to_send();
            send_packet->packet_type = PACKET_RESUME;
            send_packet->resume.input_buffer = tVBOpt.INPUT_BUFFER;
            ship_packet(send_packet);
            return;
        case MULTI_MENU_RESET:
            if (areyousure(&text_areyousure_reset)) {
                // clear screen buffer
                for (int i = 0; i < 2; i++) {
                    C2D_TargetClear(screenTargetHard[i], 0);
                    C2D_TargetClear(finalScreen[i], 0);
                }
                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                video_flush(true);
                C3D_FrameEnd(0);
                guiop = AKILL | VBRESET;

                while (wait_for_free_send_slot(1000000000)) {
                    if (udsWaitConnectionStatusEvent(false, false)) {
                        udsConnectionStatus status;
                        udsGetConnectionStatus(&status);
                        if (status.total_nodes < 2 || status.status == 11) {
                            C3D_FrameEnd(0);
                            local_disconnect();
                            udsExit();
                            [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
                        }
                    }
                }
                Packet *send_packet = new_packet_to_send();
                send_packet->packet_type = PACKET_RESET;
                send_packet->resume.input_buffer = tVBOpt.INPUT_BUFFER;
                ship_packet(send_packet);
                return;
            } else {
                [[gnu::musttail]] return multiplayer_menu(MULTI_MENU_RESET);
            }
        case MULTI_MENU_BUFFER_MINUS:
            if (tVBOpt.INPUT_BUFFER > 1) tVBOpt.INPUT_BUFFER--;
            [[gnu::musttail]] return multiplayer_menu(MULTI_MENU_BUFFER_MINUS);
        case MULTI_MENU_BUFFER_PLUS:
            if (tVBOpt.INPUT_BUFFER < INPUT_BUFFER_MAX) tVBOpt.INPUT_BUFFER++;
            [[gnu::musttail]] return multiplayer_menu(MULTI_MENU_BUFFER_PLUS);
        case MULTI_MENU_LEAVE:
            if (areyousure(&text_areyousure_leave)) {
                is_multiplayer = false;
                local_disconnect();
                udsExit();
                v810_endmultiplayer();
                [[gnu::musttail]] return main_menu(MAIN_MENU_LOAD_ROM);
            } else {
                [[gnu::musttail]] return multiplayer_menu(MULTI_MENU_LEAVE);
            }
        case MULTI_MENU_OPTIONS:
            options(0);
            [[gnu::musttail]] return multiplayer_menu(MULTI_MENU_OPTIONS);
    }
}

static void multiplayer_wait_for_peer(void) {
    char player_name[11] = {0};
    udsNodeInfo peer_info;
    udsGetNodeInformation(!my_player_id + 1, &peer_info);
    udsGetNodeInfoUsername(&peer_info, player_name);
    char message[32];
    snprintf(message, sizeof(message), "%s paused the game.", player_name);
    C2D_Text text;
    C2D_TextBufClear(dynamic_textbuf);
    C2D_TextParse(&text, dynamic_textbuf, message);
    C2D_TextOptimize(&text);
    LOOP_BEGIN(no_buttons, -1);
        if (udsWaitConnectionStatusEvent(false, false)) {
            udsConnectionStatus status;
            udsGetConnectionStatus(&status);
            if (status.total_nodes < 2 || status.status == 11) {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
            }
        }
        Packet *recv_packet;
        if ((recv_packet = read_next_packet())) {
            if (recv_packet->packet_type == PACKET_RESUME || recv_packet->packet_type == PACKET_RESET) {
                if (recv_packet->packet_type == PACKET_RESET) {
                    // clear screen buffer
                    for (int i = 0; i < 2; i++) {
                        C2D_TargetClear(screenTargetHard[i], 0);
                        C2D_TargetClear(finalScreen[i], 0);
                    }
                    video_flush(true);
                    guiop = AKILL | VBRESET;
                }
                if (recv_packet->resume.input_buffer <= INPUT_BUFFER_MAX) {
                    tVBOpt.INPUT_BUFFER = recv_packet->resume.input_buffer;
                }
                loop = false;
            }
        }
        C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.7, 0.7, TINT_COLOR);
    LOOP_END(no_buttons);
}

int strptrcmp(const void *s1, const void *s2) {
    return strcasecmp(*(const char**)s1, *(const char**)s2);
}

static bool rom_loader(char *message) {
    static char path[300];
    if(!path[0]) {
        if(tVBOpt.ROM_PATH[0]) { snprintf(path,sizeof(path),"%s",tVBOpt.ROM_PATH); char *slash=strrchr(path,'/'); if(slash) slash[1]=0; }
        else snprintf(path,sizeof(path),"%s",access("sdmc:/vb/",F_OK)==0?"sdmc:/vb/":access("sdmc:/red-viper/",F_OK)==0?"sdmc:/red-viper/":"sdmc:/");
    }
    int page=0;
    for(;;) {
        DIR *directory=opendir(path);
        if(!directory) { snprintf(path,sizeof(path),"sdmc:/"); directory=opendir(path); }
        if(!directory) { rvc_message("SD CARD ERROR","CANNOT OPEN SD CARD","CHECK CARD AND TRY AGAIN"); return false; }
        char **entries=NULL; int count=0; struct dirent *dp;
        bool allocation_failed=false;
        while((dp=readdir(directory))) {
            if(dp->d_name[0]=='.') continue;
            archive_dir_t *ad=(archive_dir_t*)directory->dirData->dirStruct;
            FS_DirectoryEntry *de=&ad->entry_data[ad->index];
            if(de->attributes & FS_ATTRIBUTE_HIDDEN) continue;
            bool isdir=de->attributes & FS_ATTRIBUTE_DIRECTORY;
            char *ext=strrchr(dp->d_name,'.');
            if(!isdir && (!ext || (strcasecmp(ext,".vb") && strcasecmp(ext,".vboy") && strcasecmp(ext,".zip")))) continue;
            char **grown=realloc(entries,(count+1)*sizeof(char*));
            if(!grown) { allocation_failed=true; break; } entries=grown;
            entries[count]=malloc(strlen(dp->d_name)+2);
            if(!entries[count]) { allocation_failed=true; break; }
            sprintf(entries[count++],"%s%s",dp->d_name,isdir?"/":"");
        }
        closedir(directory); qsort(entries,count,sizeof(char*),strptrcmp);
        if(allocation_failed) { for(int i=0;i<count;i++) { free(entries[i]); }
        free(entries); rvc_message("MEMORY ERROR","CANNOT LIST THIS FOLDER","TRY A SMALLER FOLDER"); return false; }
        int pages=(count+3)/4; if(!pages) pages=1; if(page>=pages) page=pages-1;
        int shown=count-page*4; if(shown>4) shown=4;
        const char *labels[7]; char visible[4][48],note[36];
        for(int i=0;i<shown;i++) { snprintf(visible[i],sizeof(visible[i]),"%.43s",entries[page*4+i]); if(strlen(entries[page*4+i])>43) strcpy(visible[i]+40,"..."); labels[i]=visible[i]; }
        labels[shown]="Parent Folder"; labels[shown+1]="Previous Page"; labels[shown+2]="Next Page";
        snprintf(note,sizeof(note),"PAGE %d / %d - %d ITEMS",page+1,pages,count);
        int choice=rvc_rom_list(labels,shown,note);
        char selected[300]={0}; if(choice>=0 && choice<shown) snprintf(selected,sizeof(selected),"%s",entries[page*4+choice]);
        for(int i=0;i<count;i++) { free(entries[i]); }
        free(entries);
        if(choice<0 || choice==shown+3) return false;
        if(choice==shown) { size_t len=strlen(path); if(len>6) { path[len-1]=0; char *slash=strrchr(path,'/'); if(slash) slash[1]=0; } page=0; continue; }
        if(choice==shown+1) { page=(page+pages-1)%pages; continue; }
        if(choice==shown+2) { page=(page+1)%pages; continue; }
        if(strlen(path)+strlen(selected)+1>=sizeof(path)) { rvc_message("PATH TOO LONG","CHOOSE A SHORTER FOLDER PATH","ROM WAS NOT LOADED"); continue; }
        strcat(path,selected); page=0;
        if(path[strlen(path)-1]=='/') continue;
        // clear screen buffer
        for (int i = 0; i < 2; i++) {
            C2D_TargetClear(screenTargetHard[i], 0);
            C2D_TargetClear(finalScreen[i], 0);
        }
        tDSPCACHE.DDSPDataState[0] = tDSPCACHE.DDSPDataState[1] = GPU_CLEAR;
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 64; j++) {
                tDSPCACHE.SoftBufWrote[i][j].min = 0xff;
                tDSPCACHE.SoftBufWrote[i][j].max = 0;
            }
        }
        C3D_FrameBegin(0);
        video_flush(true);
        C3D_FrameEnd(0);

        // reload global settings in advance
        if (tVBOpt.GAME_SETTINGS) loadFileOptions();

        strcpy(tVBOpt.ROM_PATH, path);
        strcpy(tVBOpt.RAM_PATH, path);
        // we know there's a dot
        strcpy(strrchr(tVBOpt.RAM_PATH, '.'), ".ram");
        saveFileOptions();
        return load_rom(message);
    }
}

static void multiplayer_main(int initial_button, bool init_uds) {
    Result res;
    if (init_uds) {
        res = udsInit(0x3000, NULL);
        if (R_FAILED(res)) {
            return multiplayer_error(res, &text_multi_init_error, true);
        }
    }
    menu_start:
    LOOP_BEGIN(multiplayer_main_buttons, initial_button);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        C2D_DrawText(&text_multi_reset_on_join, C2D_AlignCenter | C2D_WithColor, 320 / 2, 140, 0, 0.7, 0.7, TINT_COLOR);
    LOOP_END(multiplayer_main_buttons);
    switch (button) {
        case MULTI_MAIN_HOST:
            if (game_running || rom_loader("Pick a game to host.")) {
                res = create_network();
                if (R_FAILED(res)) {
                    udsExit();
                    [[gnu::musttail]] return multiplayer_error(res, &text_multi_init_error, true);
                } else {
                    [[gnu::musttail]] return multiplayer_host();
                }
            } else {
                goto menu_start;
            }
        case MULTI_MAIN_JOIN:
            [[gnu::musttail]] return multiplayer_join();
        case MULTI_MAIN_BACK:
            udsExit();
            is_multiplayer = false;
            [[gnu::musttail]] return main_menu(MAIN_MENU_MULTI);
    }
}

static void multiplayer_host() {
    C2D_Text rom_text, version_text;
    C2D_TextBufClear(dynamic_textbuf);
    char buf[96];
    NetAppData appdata;
    udsGetApplicationData(&appdata, sizeof(appdata), NULL);
    appdata.rom_name[sizeof(appdata.rom_name) - 1] = 0;
    snprintf(buf, sizeof(buf), "%s\nCRC32: %08lX", appdata.rom_name, tVBOpt.CRC32);
    C2D_TextParse(&rom_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&rom_text);
    C2D_TextParse(&version_text, dynamic_textbuf, "Red Viper " VERSION);
    C2D_TextOptimize(&version_text);

    bool connected = false;
    bool dlplay = false;
    int dlplay_progress = -1;

    multiplayer_host_buttons[MULTI_HOST_CHANGE].hidden = tVBOpt.FORWARDER;

    udsConnectionStatus status;
    LOOP_BEGIN(multiplayer_host_buttons, -1);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        if (udsWaitConnectionStatusEvent(false, false)) {
            udsGetConnectionStatus(&status);
            if (!connected && status.total_nodes > 1) {
                udsSetNewConnectionsBlocked(true, true, false);
                connected = true;
                multiplayer_host_buttons[MULTI_HOST_CHANGE].hidden = true;
            } else if (connected && (status.total_nodes < 2 || status.status == 11)) {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
            }
        }

        if (connected) {
            if (dlplay_progress < 0) {
                Packet *read_packet = read_next_packet();
                if (read_packet) {
                    if (read_packet->packet_type == PACKET_LOADED) {
                        C3D_FrameEnd(0);
                        [[gnu::musttail]] return multiplayer_ready_room(true, 0);
                    } else if (read_packet->packet_type == PACKET_DLPLAY_RQ) {
                        Packet *send_packet = new_packet_to_send();
                        send_packet->packet_type = PACKET_DLPLAY_SIZE;
                        send_packet->dlplay_size.rom_size = V810_ROM1.size;
                        ship_packet(send_packet);
                        dlplay_progress = 0;
                    }
                }
            }
            if (dlplay_progress >= 0) {
                Packet *send_packet;
                while (dlplay_progress < V810_ROM1.size && (send_packet = new_packet_to_send())) {
                    send_packet->packet_type = PACKET_DATA;
                    memcpy(send_packet->data.data, V810_ROM1.pmemory + dlplay_progress, sizeof(send_packet->data.data));
                    dlplay_progress += sizeof(send_packet->data.data);
                    ship_packet(send_packet);
                }

                // once they're done they'll send a PACKET_LOADED
                // also always read packets to ensure the buffer is clear of nops
                Packet *read_packet = read_next_packet();
                if (read_packet && read_packet->packet_type == PACKET_LOADED) {
                    C3D_FrameEnd(0);
                    [[gnu::musttail]] return multiplayer_ready_room(true, 0);
                }
            }
        }

        if (!connected) {
            C2D_DrawText(&text_multi_waiting, C2D_AlignCenter | C2D_WithColor, 320 / 2, 60, 0, 0.7, 0.7, TINT_COLOR);
        } else {
            C2D_DrawText(&text_multi_preparing, C2D_AlignCenter | C2D_WithColor, 320 / 2, 60, 0, 0.7, 0.7, TINT_COLOR);
        }
        C2D_DrawText(&rom_text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 100, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&version_text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 150, 0, 0.5, 0.5, TINT_COLOR);
    LOOP_END(multiplayer_host_buttons);
    local_disconnect();
    if (button == MULTI_HOST_CHANGE && rom_loader("Pick a game to host.") && game_running) {
        Result res = create_network();
        if (R_FAILED(res)) {
            udsExit();
            [[gnu::musttail]] return multiplayer_error(res, &text_multi_init_error, true);
        } else {
            [[gnu::musttail]] return multiplayer_host();
        }
    } else {
        [[gnu::musttail]] return multiplayer_main(MULTI_MAIN_HOST, false);
    }
}

static void multiplayer_forwarder_crc32_mismatch(NetAppData *appdata) {
    char buf[96];
    C2D_Text my_game_text;
    C2D_Text my_crc32_text;
    C2D_Text their_game_text;
    C2D_Text their_crc32_text;

    appdata->rom_name[sizeof(appdata->rom_name) - 1] = 0;

    C2D_TextBufClear(dynamic_textbuf);

    snprintf(buf, sizeof(buf), "Their game: %s", appdata->rom_name);
    C2D_TextParse(&their_game_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&their_game_text);

    snprintf(buf, sizeof(buf), "CRC32: %08lX", appdata->rom_crc32);
    C2D_TextParse(&their_crc32_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&their_crc32_text);

    char *filename = strrchr(tVBOpt.ROM_PATH, '/');
    if (filename) filename++;
    else filename = tVBOpt.ROM_PATH;
    snprintf(buf, sizeof(buf), "Your game: %s", filename);
    char *lastdot = strrchr(buf, '.');
    if (lastdot) *lastdot = 0;
    C2D_TextParse(&my_game_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&my_game_text);

    snprintf(buf, sizeof(buf), "CRC32: %08lX", tVBOpt.CRC32);
    C2D_TextParse(&my_crc32_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&my_crc32_text);

    LOOP_BEGIN(multiplayer_error_buttons, 0);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        C2D_DrawText(&text_multi_no_match, C2D_AlignCenter | C2D_WithColor, 320/2, 20, 0, 0.7, 0.7, TINT_COLOR);
        C2D_DrawText(&my_game_text, C2D_AlignCenter | C2D_WithColor, 320/2, 55, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&my_crc32_text, C2D_AlignCenter | C2D_WithColor, 320/2, 70, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&their_game_text, C2D_AlignCenter | C2D_WithColor, 320/2, 90, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&their_crc32_text, C2D_AlignCenter | C2D_WithColor, 320/2, 105, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&text_forwarder_nomatch, C2D_AlignCenter | C2D_WithColor, 320/2, 140, 0, 0.5, 0.5, TINT_COLOR);
    LOOP_END(multiplayer_error_buttons);

    [[gnu::musttail]] return multiplayer_join();
}

static bool multiplayer_areyousure_version(char *their_version) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Their version: %s", their_version);
    C2D_Text their_version_text;
    C2D_TextBufClear(dynamic_textbuf);
    C2D_TextParse(&their_version_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&their_version_text);
    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN false
    LOOP_BEGIN(areyousure_buttons, AREYOUSURE_NO);
        C2D_DrawText(&text_version_mismatch, C2D_AlignCenter | C2D_WithColor, 320 / 2, 35, 0, 0.7, 0.7, TINT_COLOR);
        C2D_DrawText(&text_your_version, C2D_AlignCenter | C2D_WithColor, 320 / 2, 105, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&their_version_text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 120, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&text_connect_anyway, C2D_AlignCenter | C2D_WithColor, 320 / 2, 140, 0, 0.7, 0.7, TINT_COLOR);
    LOOP_END(areyousure_buttons);
    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN
    return button == AREYOUSURE_YES;
}

static void multiplayer_join() {
    udsNetworkScanInfo *networks;
    size_t total_networks;
    scan_beacons(&networks, &total_networks);

    C2D_TextBufClear(dynamic_textbuf);

    static C2D_Text info_texts[MULTI_JOIN_COUNT];

    for (int i = 0; i < MULTI_JOIN_COUNT; i++) {
        multiplayer_join_buttons[i].hidden = i >= total_networks;
        if (!multiplayer_join_buttons[i].hidden) {
            char name_text[11] = {0};
            udsGetNodeInfoUsername(&networks[i].nodes[0], name_text);
            C2D_TextParse(&multiplayer_join_buttons[i].text, dynamic_textbuf, name_text);
            C2D_TextOptimize(&multiplayer_join_buttons[i].text);

            NetAppData appdata;
            size_t real_appdata_size;
            udsGetNetworkStructApplicationData(&networks[i].network, &appdata, sizeof(appdata), &real_appdata_size);
            char *info_text;
            if (real_appdata_size != sizeof(appdata) || appdata.protocol_version != PROTOCOL_VERSION) {
                info_text = "Version mismatch!";
            } else {
                // just in case
                appdata.rom_name[sizeof(appdata.rom_name)-1] = 0;
                info_text = appdata.rom_name;
            }
            multiplayer_join_buttons[i].show_toggle = true;
            multiplayer_join_buttons[i].toggle = false;
            multiplayer_join_buttons[i].toggle_text_off = &info_texts[i];
            C2D_TextParse(&info_texts[i], dynamic_textbuf, info_text);
            C2D_TextOptimize(&info_texts[i]);
        }
    }

    C2D_Text version_text;
    C2D_TextParse(&version_text, dynamic_textbuf, VERSION);
    C2D_TextOptimize(&version_text);

    LOOP_BEGIN(multiplayer_join_buttons, 0);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        C2D_DrawText(&version_text, C2D_AlignCenter | C2D_WithColor, 320/2, 220, 0, 0.5, 0.5, TINT_COLOR);
    LOOP_END(multiplayer_join_buttons);
    if (button < MULTI_JOIN_COUNT) {
        // static to aid calls to multiplayer_forwarder_crc32_mismatch
        static NetAppData appdata;
        size_t real_appdata_size;
        udsGetNetworkStructApplicationData(&networks[button].network, &appdata, sizeof(appdata), &real_appdata_size);
        appdata.emulator_version[sizeof(appdata.emulator_version) - 1] = 0;
        if (real_appdata_size != sizeof(appdata) || appdata.protocol_version != PROTOCOL_VERSION) {
            udsExit();
            [[gnu::musttail]] return multiplayer_error(0, &text_protocol_mismatch, true);
        } else if (tVBOpt.FORWARDER && tVBOpt.CRC32 != appdata.rom_crc32) {
            [[gnu::musttail]] return multiplayer_forwarder_crc32_mismatch(&appdata);
        } else if (strcmp(appdata.emulator_version, VERSION) != 0 && !multiplayer_areyousure_version(appdata.emulator_version)) {
            [[gnu::musttail]] return multiplayer_join();
        } else {
            Result res = connect_to_network(&networks[button].network);
            if (R_FAILED(res)) {
                udsExit();
                [[gnu::musttail]] return multiplayer_error(res, &text_multi_comm_error, true);
            } else {
                udsGetApplicationData(&appdata, sizeof(appdata), &real_appdata_size);
                if (real_appdata_size != sizeof(appdata) || appdata.protocol_version != PROTOCOL_VERSION) {
                    local_disconnect();
                    udsExit();
                    [[gnu::musttail]] return multiplayer_error(1, &text_multi_comm_error, true);
                } else if (game_running && appdata.rom_crc32 == tVBOpt.CRC32) {
                    Packet *send_packet = new_packet_to_send();
                    send_packet->packet_type = PACKET_LOADED;
                    ship_packet(send_packet);
                    [[gnu::musttail]] return multiplayer_ready_room(false, 0);
                } else if (tVBOpt.FORWARDER) {
                    local_disconnect();
                    [[gnu::musttail]] return multiplayer_forwarder_crc32_mismatch(&appdata);
                } else {
                    [[gnu::musttail]] return multiplayer_prepare_join(0);
                }
            }
        }
    } else if (button == MULTI_JOIN_REFRESH) {
        [[gnu::musttail]] return multiplayer_join();
    } else if (button == MULTI_JOIN_BACK) {
        [[gnu::musttail]] return multiplayer_main(MULTI_MAIN_JOIN, false);
    }
}

static void multiplayer_prepare_join(int initial_button) {
    char buf[96];
    C2D_Text my_game_text;
    C2D_Text my_crc32_text;
    C2D_Text their_game_text;
    C2D_Text their_crc32_text;

    Packet *send_packet;

    if (tVBOpt.FORWARDER) {
        multiplayer_prepare_join_buttons[MULTI_PREPARE_SD].hidden = true;
        multiplayer_prepare_join_buttons[MULTI_PREPARE_DLPLAY].hidden = true;
    }

    NetAppData appdata;
    udsGetApplicationData(&appdata, sizeof(appdata), NULL);
    appdata.rom_name[sizeof(appdata.rom_name) - 1] = 0;

    C2D_TextBufClear(dynamic_textbuf);

    snprintf(buf, sizeof(buf), "Their game: %s", appdata.rom_name);
    C2D_TextParse(&their_game_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&their_game_text);

    snprintf(buf, sizeof(buf), "CRC32: %08lX", appdata.rom_crc32);
    C2D_TextParse(&their_crc32_text, dynamic_textbuf, buf);
    C2D_TextOptimize(&their_crc32_text);

    if (game_running) {
        char *filename = strrchr(tVBOpt.ROM_PATH, '/');
        if (filename) filename++;
        else filename = tVBOpt.ROM_PATH;
        snprintf(buf, sizeof(buf), "Your game: %s", filename);
        char *lastdot = strrchr(buf, '.');
        if (lastdot) *lastdot = 0;
        C2D_TextParse(&my_game_text, dynamic_textbuf, buf);
        C2D_TextOptimize(&my_game_text);

        snprintf(buf, sizeof(buf), "CRC32: %08lX", tVBOpt.CRC32);
        C2D_TextParse(&my_crc32_text, dynamic_textbuf, buf);
        C2D_TextOptimize(&my_crc32_text);
    }

    LOOP_BEGIN(multiplayer_prepare_join_buttons, initial_button);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        if (udsWaitConnectionStatusEvent(false, false)) {
            udsConnectionStatus status;
            udsGetConnectionStatus(&status);
            if (status.total_nodes < 2 || status.status == 11) {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
            }
        }

        if (game_running) {
            C2D_DrawText(&text_multi_no_match, C2D_AlignCenter | C2D_WithColor, 320/2, 20, 0, 0.7, 0.7, TINT_COLOR);
            C2D_DrawText(&my_game_text, C2D_AlignCenter | C2D_WithColor, 320/2, 55, 0, 0.5, 0.5, TINT_COLOR);
            C2D_DrawText(&my_crc32_text, C2D_AlignCenter | C2D_WithColor, 320/2, 70, 0, 0.5, 0.5, TINT_COLOR);
        } else {
            C2D_DrawText(&text_multi_not_loaded, C2D_AlignCenter | C2D_WithColor, 320/2, 30, 0, 0.7, 0.7, TINT_COLOR);
        }
        C2D_DrawText(&their_game_text, C2D_AlignCenter | C2D_WithColor, 320/2, 90, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&their_crc32_text, C2D_AlignCenter | C2D_WithColor, 320/2, 105, 0, 0.5, 0.5, TINT_COLOR);

        C2D_DrawText(&text_dlplay_saving, C2D_AlignRight | C2D_WithColor, 310, 200, 0, 0.5, 0.5, TINT_COLOR);
    LOOP_END(multiplayer_prepare_join_buttons);

    switch (button) {
        case MULTI_PREPARE_SD:
            snprintf(buf, sizeof(buf), "Please load: %s", appdata.rom_name);
            if (rom_loader(buf) && tVBOpt.CRC32 == appdata.rom_crc32) {
                send_packet = new_packet_to_send();
                send_packet->packet_type = PACKET_LOADED;
                ship_packet(send_packet);
                [[gnu::musttail]] return multiplayer_ready_room(false, 0);
            } else {
                [[gnu::musttail]] return multiplayer_prepare_join(MULTI_PREPARE_SD);
            }
            break;
        case MULTI_PREPARE_DLPLAY:
            send_packet = new_packet_to_send();
            send_packet->packet_type = PACKET_DLPLAY_RQ;
            ship_packet(send_packet);
            [[gnu::musttail]] return multiplayer_dlplay();
        case MULTI_PREPARE_LEAVE:
            local_disconnect();
            [[gnu::musttail]] return multiplayer_main(MULTI_MAIN_JOIN, false);
    }
}

static void multiplayer_dlplay(void) {
    NetAppData appdata;
    udsGetApplicationData(&appdata, sizeof(appdata), NULL);
    appdata.rom_name[sizeof(appdata.rom_name) - 1] = 0;

    game_running = false;
    bool size_received = false;
    int dlplay_progress = 0;
    LOOP_BEGIN(multiplayer_dlplay_buttons, -1);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        if (udsWaitConnectionStatusEvent(false, false)) {
            udsConnectionStatus status;
            udsGetConnectionStatus(&status);
            if (status.total_nodes < 2 || status.status == 11) {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
            }
        }
        bool packet_received = false;
        Packet *recv_packet;
        if (!size_received && (recv_packet = read_next_packet())) {
            packet_received = true;
            if (recv_packet->packet_type == PACKET_DLPLAY_SIZE) {
                int rom_size = recv_packet->dlplay_size.rom_size;
                if (rom_size > MAX_ROM_SIZE || ((rom_size - 1) & rom_size)) {
                    C3D_FrameEnd(0);
                    local_disconnect();
                    udsExit();
                    [[gnu::musttail]] return multiplayer_error(0, &text_multi_init_error, true);
                }
                V810_ROM1.size = rom_size;
                V810_ROM1.highaddr = 0x7000000 + rom_size - 1;
                size_received = true;
            }
        }
        while (size_received && (recv_packet = read_next_packet())) {
            packet_received = true;
            if (recv_packet->packet_type == PACKET_DATA) {
                memcpy(V810_ROM1.pmemory + dlplay_progress, recv_packet->data.data, sizeof(recv_packet->data.data));
                dlplay_progress += sizeof(recv_packet->data.data);
            }
        }
        if (packet_received && send_queue_empty()) {
            Packet *send_packet = new_packet_to_send();
            send_packet->packet_type = PACKET_NOP;
            ship_packet(send_packet);
        }
        if (size_received && dlplay_progress == V810_ROM1.size) {
            // fill the rest of the address space with copies of the rom
            for (int i = V810_ROM1.size; i < MAX_ROM_SIZE; i += V810_ROM1.size) {
                memcpy(V810_ROM1.pmemory + i, V810_ROM1.pmemory, V810_ROM1.size);
            }
            tVBOpt.RAM_PATH[0] = 0;
            game_running = true;
            is_sram = false;
            gen_table();
            tVBOpt.CRC32 = get_crc(V810_ROM1.size);
            tVBOpt.GAME_ID = MAKE_GAMEID((char*)(V810_ROM1.off + (V810_ROM1.highaddr & 0xFFFFFDF9)));
            apply_patches();
            v810_reset();

            Packet *send_packet = new_packet_to_send();
            send_packet->packet_type = PACKET_LOADED;
            ship_packet(send_packet);

            C3D_FrameEnd(0);
            [[gnu::musttail]] return multiplayer_ready_room(false, 0);
        }

        if (size_received) C2D_DrawRectSolid(60, 140, 0, 200, 16, C2D_Color32(0.5 * TINT_R, 0.5 * TINT_G, 0.5 * TINT_B, 255));
        if (size_received) {
            C2D_DrawRectSolid(60, 140, 0, 200 * dlplay_progress / V810_ROM1.size, 16, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
        }

        C2D_TextBufClear(dynamic_textbuf);


        C2D_DrawText(&text_downloading, C2D_AlignCenter | C2D_WithColor, 320/2, 60, 0, 0.7, 0.7, TINT_COLOR);

        C2D_Text game_name_text;
        C2D_TextParse(&game_name_text, dynamic_textbuf, appdata.rom_name);
        C2D_TextOptimize(&game_name_text);
        C2D_DrawText(&game_name_text, C2D_AlignCenter | C2D_WithColor, 320/2, 85, 0, 0.5, 0.5, TINT_COLOR);

        C2D_Text progress_text;
        char progress_buf[20];
        snprintf(progress_buf, sizeof(progress_buf), "Progress: %5.2f%%", dlplay_progress * 100.0 / V810_ROM1.size);
        C2D_TextParse(&progress_text, dynamic_textbuf, progress_buf);
        C2D_TextOptimize(&progress_text);
        C2D_DrawText(&progress_text, C2D_AlignLeft | C2D_WithColor, 320/2 - 80, 110, 0, 0.7, 0.7, TINT_COLOR);
    LOOP_END(multiplayer_dlplay_buttons);
    local_disconnect();
    udsExit();
    [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
}

static void multiplayer_sram_transfer() {
    int send_progress = 0;
    int recv_progress = 0;
    if (my_player_id == 1) {
        memcpy(vb_players[1].V810_GAME_RAM.pmemory, vb_players[0].V810_GAME_RAM.pmemory, vb_players[0].V810_GAME_RAM.size);
    }
    LOOP_BEGIN(no_buttons, -1);
        if (udsWaitConnectionStatusEvent(false, false)) {
            udsConnectionStatus status;
            udsGetConnectionStatus(&status);
            if (status.total_nodes < 2 || status.status == 11) {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
            }
        }
        Packet *send_packet;
        while (send_progress < vb_state->V810_GAME_RAM.size && (send_packet = new_packet_to_send())) {
            send_packet->packet_type = PACKET_DATA;
            memcpy(send_packet->data.data, vb_players[my_player_id].V810_GAME_RAM.pmemory + send_progress, sizeof(send_packet->data.data));
            send_progress += sizeof(send_packet->data.data);
            ship_packet(send_packet);
        }
        bool recvd_packet = false;
        Packet *recv_packet;
        while (recv_progress < vb_state->V810_GAME_RAM.size && (recv_packet = read_next_packet())) {
            recvd_packet = true;
            if (recv_packet->packet_type == PACKET_DATA) {
                memcpy(vb_players[!my_player_id].V810_GAME_RAM.pmemory + recv_progress, recv_packet->data.data, sizeof(recv_packet->data.data));
                recv_progress += sizeof(recv_packet->data.data);
            } else {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(1, &text_multi_comm_error, true);
            }
        }
        // send nops so any acks can get through
        if (!recvd_packet && send_queue_empty() && (send_packet = new_packet_to_send())) {
            send_packet->packet_type = PACKET_NOP;
            ship_packet(send_packet);
        }
        C2D_DrawRectSolid(60, 140, 0, 200, 16, C2D_Color32(0.5 * TINT_R, 0.5 * TINT_G, 0.5 * TINT_B, 255));
        C2D_DrawRectSolid(60, 140, 0, 200 * recv_progress / vb_state->V810_GAME_RAM.size, 16, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
        if (send_progress == vb_state->V810_GAME_RAM.size && recv_progress == vb_state->V810_GAME_RAM.size) {
            loop = false;
            guiop = AKILL | VBRESET;
        }
    LOOP_END(no_buttons);
}

static void multiplayer_ready_room(bool is_host, int initial_button) {
    multiplayer_ready_room_buttons[MULTI_READY_BUFFER_MINUS].hidden = !is_host;
    multiplayer_ready_room_buttons[MULTI_READY_BUFFER_PLUS].hidden = !is_host;

    char player_name[11] = {0};
    char player_str[16];
    C2D_Text p1_text;
    C2D_Text p2_text;
    C2D_Text buffer_text;
    multiplayer_ready_room_buttons[MULTI_READY_START].hidden = !is_host;
    {
        udsConnectionStatus status;
        udsGetConnectionStatus(&status);
        is_multiplayer = true;
        my_player_id = status.cur_NetworkNodeID - 1;
    }
    C2D_TextBufClear(dynamic_textbuf);
    udsNodeInfo node_info = {0};

    udsGetNodeInformation(1, &node_info);
    udsGetNodeInfoUsername(&node_info, player_name);
    snprintf(player_str, sizeof(player_str), "P1: %s", player_name);
    C2D_TextParse(&p1_text, dynamic_textbuf, player_str);
    C2D_TextOptimize(&p1_text);

    memset(player_name, 0, sizeof(player_name));
    udsGetNodeInformation(2, &node_info);
    udsGetNodeInfoUsername(&node_info, player_name);
    snprintf(player_str, sizeof(player_str), "P2: %s", player_name);
    C2D_TextParse(&p2_text, dynamic_textbuf, player_str);
    C2D_TextOptimize(&p2_text);

    snprintf(player_str, sizeof(player_str), "%d", tVBOpt.INPUT_BUFFER);
    C2D_TextParse(&buffer_text, dynamic_textbuf, player_str);
    C2D_TextOptimize(&buffer_text);

    LOOP_BEGIN(multiplayer_ready_room_buttons, initial_button);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        if (udsWaitConnectionStatusEvent(false, false)) {
            udsConnectionStatus status;
            udsGetConnectionStatus(&status);
            if (status.total_nodes < 2 || status.status == 11) {
                C3D_FrameEnd(0);
                local_disconnect();
                udsExit();
                [[gnu::musttail]] return multiplayer_error(0, &text_multi_disconnect, true);
            }
        }
        if (!is_host) {
            Packet *packet = read_next_packet();
            if (packet) {
                if (packet->packet_type == PACKET_RESUME) {
                    if (packet->resume.input_buffer <= INPUT_BUFFER_MAX) {
                        tVBOpt.INPUT_BUFFER = packet->resume.input_buffer;
                    }
                    loop = false;
                    button = MULTI_READY_START;
                }
            }
        }
        C2D_DrawText(&p1_text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.7, 0.7, TINT_COLOR);
        C2D_DrawText(&p2_text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 110, 0, 0.7, 0.7, TINT_COLOR);
        if (is_host) {
            C2D_DrawText(&text_input_buffer, C2D_AlignRight | C2D_WithColor, 180 - 8, 144, 0, 0.7, 0.7, TINT_COLOR);
            C2D_DrawText(&buffer_text, C2D_AlignCenter | C2D_WithColor, 180 + 48, 144, 0, 0.7, 0.7, TINT_COLOR);
        }
    LOOP_END(multiplayer_ready_room_buttons);
    switch (button) {
        case MULTI_READY_START:
            if (is_host) {
                // should be ok if we haven't sent anything yet
                Packet *packet = new_packet_to_send();
                packet->packet_type = PACKET_RESUME;
                packet->resume.input_buffer = tVBOpt.INPUT_BUFFER;
                ship_packet(packet);
            }
            [[gnu::musttail]] return multiplayer_sram_transfer();
        case MULTI_READY_BUFFER_MINUS:
            if (tVBOpt.INPUT_BUFFER > 1) tVBOpt.INPUT_BUFFER--;
            [[gnu::musttail]] return multiplayer_ready_room(is_host, MULTI_READY_BUFFER_MINUS);
        case MULTI_READY_BUFFER_PLUS:
            if (tVBOpt.INPUT_BUFFER < INPUT_BUFFER_MAX) tVBOpt.INPUT_BUFFER++;
            [[gnu::musttail]] return multiplayer_ready_room(is_host, MULTI_READY_BUFFER_PLUS);
        case MULTI_READY_LEAVE:
            local_disconnect();
            [[gnu::musttail]] return multiplayer_main(is_host ? MULTI_MAIN_HOST : MULTI_MAIN_JOIN, false);
    }
}

static void multiplayer_error(int err, C2D_Text *message, bool exit_to_menu) {
    C2D_Text text;
    char code_message[32];
    snprintf(code_message, sizeof(code_message), "Error code: %d", err);
    C2D_TextParse(&text, dynamic_textbuf, code_message);
    C2D_TextOptimize(&text);
    LOOP_BEGIN(multiplayer_error_buttons, 0);
        if(!game_running) draw_logo();
        rvc_ui_frame("MULTIPLAYER");
        C2D_DrawText(message, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.5, 0.5, TINT_COLOR);
        if (err != 0) C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 120, 0, 0.5, 0.5, TINT_COLOR);
    LOOP_END(multiplayer_error_buttons);
    is_multiplayer = false;
    if (exit_to_menu) [[gnu::musttail]] return main_menu(MAIN_MENU_MULTI);
}

static void controls(int initial_button) {
    bool newer=false; APT_CheckNew3DS(&newer);
    int focus=initial_button;
    for(;;) {
        const char *labels[]={tVBOpt.CUSTOM_CONTROLS?"Scheme: Custom":"Scheme: Preset","Configure Scheme","Touchscreen Settings",tVBOpt.INPUTS?"Input Display: On":"Input Display: Off","Circle Pad Pro"};
        int count=newer?4:5;
        int b=rvc_list("CONTROLS",labels,count,focus,"Back",NULL);
        if(b<0 || b==count) { tVBOpt.CUSTOM_CONTROLS?setCustomControls():setPresetControls(buttons_on_screen); return; }
        focus=b;
        if(b==0) { tVBOpt.CUSTOM_CONTROLS=!tVBOpt.CUSTOM_CONTROLS; tVBOpt.MODIFIED=true; }
        if(b==1) { if(tVBOpt.CUSTOM_CONTROLS) custom_3ds_mappings(CUSTOM_3DS_MAPPINGS_BACK); else preset_controls(0); }
        if(b==2) touchscreen_settings();
        if(b==3) { tVBOpt.INPUTS=!tVBOpt.INPUTS; tVBOpt.MODIFIED=true; }
        if(b==4) cpp_options(0);
        if(guiop & GUIEXIT) return;
    }
}

static void cpp_options(int initial_button) {
    int focus=0;
    for(;;) {
        const char *labels[]={tVBOpt.CPP_ENABLED?"CPP Enabled: On":"CPP Enabled: Off","Calibrate CPP"};
        int b=rvc_list("CIRCLE PAD PRO",labels,2,focus,"Back",cppGetConnected()?"CONNECTED":"NOT CONNECTED");
        if(b<0 || b==2) return;
        focus=b;
        if(b==0) { tVBOpt.CPP_ENABLED=!tVBOpt.CPP_ENABLED;tVBOpt.MODIFIED=true;if(tVBOpt.CPP_ENABLED) cppInit();else cppExit(); }
        if(b==1) {
            toggleVsync(false);if(tVBOpt.CPP_ENABLED) cppExit();
            extraPadConf conf;extraPadInit(&conf);extraPadLaunch(&conf);
            if(tVBOpt.CPP_ENABLED) {cppInit();}
            toggleVsync(tVBOpt.VSYNC);
        }
    }
}

static C2D_Text * vb_button_code_to_vb_button_text(int vb_button_code) {
    switch (vb_button_code) {
        #define VB_BUTTON_CODE_TO_TEXT_CASE(VB_BUTTON) \
        case VB_##VB_BUTTON: \
            return &text_custom_vb_button_##VB_BUTTON;
        PERFORM_FOR_EACH_VB_BUTTON(VB_BUTTON_CODE_TO_TEXT_CASE)
        default:
            return &text_error;
    }
}

#define DRAW_CUSTOM_3DS_BUTTON_FUNCTION(CUSTOM_3DS_BUTTON) \
static void draw_custom_3ds_##CUSTOM_3DS_BUTTON(Button *self) { \
    C2D_DrawText(&text_custom_3ds_button_##CUSTOM_3DS_BUTTON, C2D_AlignLeft, self->x + 5, self->y + 3, 0, 0.6, 0.6); \
    C2D_DrawRectSolid(self->x + 12, self->y + self->h / 2, 0, self->w - 24, 1, BLACK); \
    C2D_DrawText(vb_button_code_to_vb_button_text(tVBOpt.CUSTOM_MAPPING_##CUSTOM_3DS_BUTTON), C2D_AlignRight, self->x + self->w - 5, self->y + self->h / 2, 0, 0.6, 0.6); \
    C2D_Sprite *sprite = NULL; \
    if (tVBOpt.CUSTOM_MOD[__builtin_ctz(KEY_##CUSTOM_3DS_BUTTON)] == 1) sprite = &text_toggle_sprite; \
    if (tVBOpt.CUSTOM_MOD[__builtin_ctz(KEY_##CUSTOM_3DS_BUTTON)] == 2) sprite = &text_turbo_sprite; \
    if (sprite) { \
        C2D_Flush(); \
        C3D_ColorLogicOp(GPU_LOGICOP_AND); \
        C2D_SpriteSetPos(sprite, self->x + 4, self->y + self->h - 9); \
        C2D_DrawSprite(sprite); \
        C2D_Flush(); \
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA); \
    } \
}
PERFORM_FOR_EACH_3DS_BUTTON(DRAW_CUSTOM_3DS_BUTTON_FUNCTION)

static int __attribute__((unused)) vb_button_code_to_vb_ui_button_index(int vb_button_code) {
    switch (vb_button_code) {
        #define VB_BUTTON_CODE_TO_INDEX_CASE(VB_BUTTON) \
        case VB_##VB_BUTTON: \
            return CUSTOM_VB_MAPPINGS_##VB_BUTTON;
        PERFORM_FOR_EACH_VB_BUTTON(VB_BUTTON_CODE_TO_INDEX_CASE)
        default:
            return CUSTOM_VB_MAPPINGS_BACK;
    }
}

static int current_custom_mapping_3ds_button;
static C2D_Text *current_custom_mapping_3ds_button_text;
static int *current_custom_mapping_vb_option;
static int *current_custom_mapping_mod;

static void custom_3ds_mappings(int initial_button) {
    bool newer=tVBOpt.CPP_ENABLED;if(!newer) APT_CheckNew3DS(&newer);
    const char *names[]={
        #define RVC_NAME(key) #key,
        PERFORM_FOR_EACH_3DS_BUTTON(RVC_NAME)
        #undef RVC_NAME
    };
    int *mapping[]={
        #define RVC_MAPPING(key) &tVBOpt.CUSTOM_MAPPING_##key,
        PERFORM_FOR_EACH_3DS_BUTTON(RVC_MAPPING)
        #undef RVC_MAPPING
    };
    unsigned codes[]={
        #define RVC_KEY(key) KEY_##key,
        PERFORM_FOR_EACH_3DS_BUTTON(RVC_KEY)
        #undef RVC_KEY
    };
    int indices[22],count=0;
    for(int i=0;i<22;i++) {
        if(!newer && (!strncmp(names[i],"CSTICK",6) || !strcmp(names[i],"ZL") || !strcmp(names[i],"ZR"))) continue;
        indices[count++]=i;
    }
    int page=0;
    for(;;) {
        const char *labels[7];char text[4][44];int shown=count-page*4;if(shown>4) shown=4;
        for(int i=0;i<shown;i++) {snprintf(text[i],44,"3DS %s",names[indices[page*4+i]]);labels[i]=text[i];}
        labels[shown]="Previous Page";labels[shown+1]="Next Page";labels[shown+2]="Reset Mappings";
        int b=rvc_list("BUTTON MAPPING",labels,shown+3,0,"Back",NULL);
        if(b<0 || b==shown+3) {setCustomControls();return;}
        if(b==shown) {page=(page+(count+3)/4-1)%((count+3)/4);continue;}
        if(b==shown+1) {page=(page+1)%((count+3)/4);continue;}
        if(b==shown+2) {setCustomMappingDefaults();tVBOpt.MODIFIED=true;continue;}
        int n=indices[page*4+b];current_custom_mapping_vb_option=mapping[n];
        current_custom_mapping_mod=&tVBOpt.CUSTOM_MOD[__builtin_ctz(codes[n])];
        custom_vb_mappings(0);
    }
}

static void custom_vb_mappings(int initial_button) {
    const char *names[]={"L","R","Select","Start","B","A","Left Pad Up","Left Pad Down","Left Pad Left","Left Pad Right","Right Pad Up","Right Pad Down","Right Pad Left","Right Pad Right"};
    const int values[]={VB_KEY_L,VB_KEY_R,VB_KEY_SELECT,VB_KEY_START,VB_KEY_B,VB_KEY_A,VB_LPAD_U,VB_LPAD_D,VB_LPAD_L,VB_LPAD_R,VB_RPAD_U,VB_RPAD_D,VB_RPAD_L,VB_RPAD_R};
    int page=0;
    for(;;) {
        const char *labels[7];int shown=14-page*4;if(shown>4) shown=4;
        for(int i=0;i<shown;i++) labels[i]=names[page*4+i];
        labels[shown]="Previous Page";labels[shown+1]="Next Page";
        labels[shown+2]=*current_custom_mapping_mod==0?"Modifier: Normal":*current_custom_mapping_mod==1?"Modifier: Toggle":"Modifier: Turbo";
        int b=rvc_list("VIRTUAL BOY INPUT",labels,shown+3,0,"Back",NULL);
        if(b<0 || b==shown+3) return;
        if(b==shown) page=(page+3)%4;
        else if(b==shown+1) page=(page+1)%4;
        else if(b==shown+2) {*current_custom_mapping_mod=(*current_custom_mapping_mod+1)%3;tVBOpt.MODIFIED=true;}
        else {*current_custom_mapping_vb_option=values[page*4+b];tVBOpt.MODIFIED=true;return;}
    }
}

static void draw_abxy(Button *self) {
    const int FACEX = self->x + self->w / 2;
    const int FACEY = self->y + self->h / 2;
    const int FACEW = self->w;
    const int FACEH = self->h;
    const int OFFSET = 22;
    // Shifting the draw x offset by 0.5 is a workaround for a Citro2D text rendering bug; it's being done to the (3DS) L, R, A, B, X, Y buttons here
    C2D_DrawText(tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 5 ? &text_btn_A : &text_btn_B, C2D_AlignCenter, FACEX - 0.5, FACEY - OFFSET - 16, 0, 1, 1);
    C2D_DrawText(tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 4 ? &text_btn_B : &text_btn_A, C2D_AlignCenter, FACEX - 0.5, FACEY + OFFSET - 16, 0, 1, 1);
    C2D_DrawText(tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 5 ? &text_btn_B : &text_btn_A, C2D_AlignCenter, FACEX - OFFSET - 0.5, FACEY - 16, 0, 1, 1);
    C2D_DrawText(tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 4 ? &text_btn_A : &text_btn_B, C2D_AlignCenter, FACEX + OFFSET - 0.5, FACEY - 16, 0, 1, 1);
}

static void draw_shoulders(Button *self) {
    const int SHOULDX = self->x + self->w / 2;
    const int SHOULDY = self->y;
    const int SHOULDW = self->w;
    const int SHOULDH = self->h;
    const int OFFSET = 22;
    // Shifting the draw x offset by 0.5 is a workaround for a Citro2D text rendering bug; it's being done to the (3DS) L, R, A, B, X, Y buttons here
    C2D_DrawText(tVBOpt.ZLZR_MODE <= 1 ? &text_btn_L : tVBOpt.ZLZR_MODE <= 2 ? &text_btn_B : &text_btn_A, C2D_AlignCenter, SHOULDX - OFFSET*2 - 0.5, SHOULDY + 5, 0, 1, 1);
    C2D_DrawText(tVBOpt.ZLZR_MODE >= 2 ? &text_btn_L : tVBOpt.ZLZR_MODE == 0 ? &text_btn_B : &text_btn_A, C2D_AlignCenter, SHOULDX - OFFSET*0.75, SHOULDY + 5, 0, 1, 1);
    C2D_DrawText(tVBOpt.ZLZR_MODE >= 2 ? &text_btn_R : tVBOpt.ZLZR_MODE == 0 ? &text_btn_A : &text_btn_B, C2D_AlignCenter, SHOULDX + OFFSET*0.75, SHOULDY + 5, 0, 1, 1);
    C2D_DrawText(tVBOpt.ZLZR_MODE <= 1 ? &text_btn_R : tVBOpt.ZLZR_MODE <= 2 ? &text_btn_A : &text_btn_B, C2D_AlignCenter, SHOULDX + OFFSET*2 + 0.5, SHOULDY + 5, 0, 1, 1);
}

static void draw_multislot(Button *self) {
    int id = (self - multicolour_picker_buttons) / 2;
    C2D_DrawRectSolid(self->x + 100 - 4 - 32 - 8 - 32, self->y + 8, 0, 32, 24, 0xff000000);
    C2D_DrawRectSolid(self->x + 100 - 4 - 32 - 8 - 32 + 1, self->y + 8 + 1, 0, 30, 22, 0xff000000 | tVBOpt.MTINT[id][0]);
    C2D_DrawRectSolid(self->x + 100 - 4 - 32, self->y + 8, 0, 32, 24, 0xff000000);
    C2D_DrawRectSolid(self->x + 100 - 4 - 32 + 1, self->y + 8 + 1, 0, 30, 22, 0xff000000 | tVBOpt.MTINT[id][1]);
    C2D_DrawRectSolid(self->x + 100 + 4, self->y + 8, 0, 32, 24, 0xff000000);
    C2D_DrawRectSolid(self->x + 100 + 4 + 1, self->y + 8 + 1, 0, 30, 22, 0xff000000 | tVBOpt.MTINT[id][2]);
    C2D_DrawRectSolid(self->x + 100 + 4 + 32 + 8, self->y + 8, 0, 32, 24, 0xff000000);
    C2D_DrawRectSolid(self->x + 100 + 4 + 32 + 8 + 1, self->y + 8 + 1, 0, 30, 22, 0xff000000 | tVBOpt.MTINT[id][3]);
    if (id == tVBOpt.MULTIID) C2D_DrawCircleSolid(self->x + 5, self->y + self->h / 2, 0, 4, 0xff000000);
}

static void preset_controls(int initial_button) {
    bool newer=tVBOpt.CPP_ENABLED;if(!newer) APT_CheckNew3DS(&newer);
    int focus=0;
    for(;;) {
        char face[48],shoulders[48];snprintf(face,sizeof(face),"Face Mapping: Preset %d",tVBOpt.ABXY_MODE+1);
        snprintf(shoulders,sizeof(shoulders),"Shoulders: Preset %d",tVBOpt.ZLZR_MODE+1);
        const char *pads[]={"D-Pad: Left Pad","D-Pad: Right Pad","D-Pad: Mirror Face Buttons"};
        const char *labels[]={face,pads[tVBOpt.DPAD_MODE],shoulders};int count=newer?3:2;
        int b=rvc_list("CONTROL PRESETS",labels,count,focus,"Back",NULL);
        if(b<0 || b==count) {setPresetControls(buttons_on_screen);return;}
        focus=b;tVBOpt.MODIFIED=true;
        if(b==0) tVBOpt.ABXY_MODE=(tVBOpt.ABXY_MODE+1)%6;
        if(b==1) tVBOpt.DPAD_MODE=(tVBOpt.DPAD_MODE+1)%3;
        if(b==2) tVBOpt.ZLZR_MODE=(tVBOpt.ZLZR_MODE+1)%4;
    }
}

static void touchscreen_editor(void);
static void touchscreen_settings(void) {
    int focus=0;
    for(;;) {
        const char *labels[]={"Move Controls",tVBOpt.TOUCH_BUTTONS?"Default View: A / B":"Default View: Right Pad",tVBOpt.TOUCH_SWITCH?"Switch Button: On":"Switch Button: Off","Reset Layout"};
        int b=rvc_list("TOUCH LAYOUT",labels,4,focus,"Back",NULL);
        if(b<0 || b==4) return;
        focus=b;
        if(b==0) touchscreen_editor();
        if(b==1) {tVBOpt.TOUCH_BUTTONS=!tVBOpt.TOUCH_BUTTONS;buttons_on_screen=tVBOpt.TOUCH_BUTTONS;tVBOpt.MODIFIED=true;}
        if(b==2) {tVBOpt.TOUCH_SWITCH=!tVBOpt.TOUCH_SWITCH;tVBOpt.MODIFIED=true;}
        if(b==3) {tVBOpt.PAUSE_RIGHT=152;tVBOpt.TOUCH_AX=259;tVBOpt.TOUCH_AY=126;tVBOpt.TOUCH_BX=194;tVBOpt.TOUCH_BY=168;tVBOpt.TOUCH_PADX=224;tVBOpt.TOUCH_PADY=130;tVBOpt.MODIFIED=true;}
    }
}
static void touchscreen_editor(void) {
    touchscreen_settings_buttons[TOUCHSCREEN_DEFAULT].hidden = buttons_on_screen == tVBOpt.TOUCH_BUTTONS;
    touchscreen_settings_buttons[TOUCHSCREEN_BACK].x=23;touchscreen_settings_buttons[TOUCHSCREEN_BACK].y=211;touchscreen_settings_buttons[TOUCHSCREEN_BACK].w=88;touchscreen_settings_buttons[TOUCHSCREEN_BACK].h=26;
    touchscreen_settings_buttons[TOUCHSCREEN_RESET].x=174;touchscreen_settings_buttons[TOUCHSCREEN_RESET].y=211;touchscreen_settings_buttons[TOUCHSCREEN_RESET].w=115;touchscreen_settings_buttons[TOUCHSCREEN_RESET].h=26;
    touchscreen_settings_buttons[TOUCHSCREEN_SWITCH].hidden=true;
    touchscreen_settings_buttons[TOUCHSCREEN_DEFAULT].hidden=true;
    // 1: pause, 2: dpad/a, 3: b
    int dragging = 0;
    int xoff = 0, yoff = 0;
    int BUTTON_RAD = 24;
    int PAD_RAD = 48;
    int PAUSE_RAD = 4;
    buttonLock = true;
    LOOP_BEGIN(touchscreen_settings_buttons, -1);
        if(!game_running) draw_logo();
        // handle dragging
        touchPosition touch_pos;
        hidTouchRead(&touch_pos);
        if (hidKeysDown() & KEY_TOUCH) {
            if (guiShouldSwitch()) {
                buttons_on_screen = !buttons_on_screen;
                touchscreen_settings_buttons[TOUCHSCREEN_DEFAULT].hidden = true;
            } else if (abs(tVBOpt.PAUSE_RIGHT - touch_pos.px) < PAUSE_RAD) {
                // pause slider
                tVBOpt.MODIFIED = true;
                dragging = 1;
                xoff = tVBOpt.PAUSE_RIGHT - touch_pos.px;
            } else if (buttons_on_screen) {
                int adx = tVBOpt.TOUCH_AX - touch_pos.px;
                int ady = tVBOpt.TOUCH_AY - touch_pos.py;
                int bdx = tVBOpt.TOUCH_BX - touch_pos.px;
                int bdy = tVBOpt.TOUCH_BY - touch_pos.py;
                if (adx*adx + ady*ady < BUTTON_RAD*BUTTON_RAD) {
                    // a button
                    tVBOpt.MODIFIED = true;
                    dragging = 2;
                    xoff = adx;
                    yoff = ady;
                } else if (bdx*bdx + bdy*bdy < BUTTON_RAD*BUTTON_RAD) {
                    // b button
                    tVBOpt.MODIFIED = true;
                    dragging = 3;
                    xoff = bdx;
                    yoff = bdy;
                }
            } else {
                int dx = tVBOpt.TOUCH_PADX - touch_pos.px;
                int dy = tVBOpt.TOUCH_PADY - touch_pos.py;
                if (dx*dx + dy*dy < PAD_RAD*PAD_RAD) {
                    // dpad
                    tVBOpt.MODIFIED = true;
                    dragging = 2;
                    xoff = dx;
                    yoff = dy;
                }
            }
        }
        if (hidKeysHeld() & KEY_TOUCH) {
            if (dragging == 1) {
                // pause slider
                int dest_x = touch_pos.px + xoff;
                if (dest_x > tVBOpt.TOUCH_AX - BUTTON_RAD - PAUSE_RAD)
                    dest_x = tVBOpt.TOUCH_AX - BUTTON_RAD - PAUSE_RAD;
                if (dest_x > tVBOpt.TOUCH_BX - BUTTON_RAD - PAUSE_RAD)
                    dest_x = tVBOpt.TOUCH_BX - BUTTON_RAD - PAUSE_RAD;
                if (dest_x > tVBOpt.TOUCH_PADX - PAD_RAD - PAUSE_RAD)
                    dest_x = tVBOpt.TOUCH_PADX - PAD_RAD - PAUSE_RAD;
                if (dest_x > 192)
                    dest_x = 192;
                if (dest_x < 80 + PAUSE_RAD)
                    dest_x = 80 + PAUSE_RAD;
                tVBOpt.PAUSE_RIGHT = dest_x;
            } else if (dragging != 0) {
                // button or dpad
                int dest_x = touch_pos.px + xoff;
                int dest_y = touch_pos.py + yoff;
                int rad = buttons_on_screen ? BUTTON_RAD : PAD_RAD;
                if (dest_x > 320 - rad)
                    dest_x = 320 - rad;
                if (dest_y > 240 - rad)
                    dest_y = 240 - rad;
                if (dest_x < tVBOpt.PAUSE_RIGHT + PAUSE_RAD + rad)
                    dest_x = tVBOpt.PAUSE_RIGHT + PAUSE_RAD + rad;
                // lower top range to account for switch button
                if (dest_y < 32 + rad)
                    dest_y = 32 + rad;
                if (buttons_on_screen) {
                    if (dragging == 2) {
                        // a button
                        int dx = dest_x - tVBOpt.TOUCH_BX;
                        int dy = dest_y - tVBOpt.TOUCH_BY;
                        int dist_sqr = dx*dx + dy*dy;
                        if (dist_sqr < 4*BUTTON_RAD*BUTTON_RAD && dist_sqr != 0) {
                            float dist = sqrt(dist_sqr);
                            dest_x = tVBOpt.TOUCH_BX + dx * BUTTON_RAD * 2 / dist;
                            dest_y = tVBOpt.TOUCH_BY + dy * BUTTON_RAD * 2 / dist;
                        }
                    } else {
                        // b button
                        int dx = dest_x - tVBOpt.TOUCH_AX;
                        int dy = dest_y - tVBOpt.TOUCH_AY;
                        int dist_sqr = dx*dx + dy*dy;
                        if (dist_sqr < 4*BUTTON_RAD*BUTTON_RAD && dist_sqr != 0) {
                            float dist = sqrt(dist_sqr);
                            dest_x = tVBOpt.TOUCH_AX + dx * BUTTON_RAD * 2 / dist;
                            dest_y = tVBOpt.TOUCH_AY + dy * BUTTON_RAD * 2 / dist;
                        }
                    }
                    // additional check in case the button check shunted us OOB
                    if (dest_x <= 320 - BUTTON_RAD && dest_x >= tVBOpt.PAUSE_RIGHT + PAUSE_RAD + BUTTON_RAD
                        && dest_y <= 240 - BUTTON_RAD && dest_y >= 32 + BUTTON_RAD
                    ) {
                        if (dragging == 2) {
                            tVBOpt.TOUCH_AX = dest_x;
                            tVBOpt.TOUCH_AY = dest_y;
                        } else {
                            tVBOpt.TOUCH_BX = dest_x;
                            tVBOpt.TOUCH_BY = dest_y;
                        }
                    }
                } else {
                    tVBOpt.TOUCH_PADX = dest_x;
                    tVBOpt.TOUCH_PADY = dest_y;
                }
            }
        } else {
            dragging = 0;
        }

        // exit on B
        if (hidKeysDown() & KEY_B) button = TOUCHSCREEN_BACK;

        // draw
        drawTouchControls(
            dragging == 2 ? VB_KEY_A :
            dragging == 3 ? VB_KEY_B :
            dragging ? VB_KEY_START : 0);
        C2D_DrawRectSolid(
            tVBOpt.PAUSE_RIGHT - PAUSE_RAD, 240/2 - 8, 0,
            PAUSE_RAD * 2, 8*2, dragging == 1 ? TINT_90 : TINT_50
        );
        rvc_ui_text("DRAG TO POSITION",160,54,true);
    LOOP_END(touchscreen_settings_buttons);
    buttonLock = false;
    switch (button) {
        case TOUCHSCREEN_BACK: // Back
            { return; }
        case TOUCHSCREEN_RESET: // Reset
            tVBOpt.MODIFIED = true;
            tVBOpt.PAUSE_RIGHT = 152;
            tVBOpt.TOUCH_AX = 259;
            tVBOpt.TOUCH_AY = 126;
            tVBOpt.TOUCH_BX = 194;
            tVBOpt.TOUCH_BY = 168;
            tVBOpt.TOUCH_PADX = 224;
            tVBOpt.TOUCH_PADY = 130;
            [[gnu::musttail]] return touchscreen_editor();
        case TOUCHSCREEN_SWITCH:
            tVBOpt.MODIFIED = true;
            tVBOpt.TOUCH_SWITCH = !tVBOpt.TOUCH_SWITCH;
            [[gnu::musttail]] return touchscreen_editor();
        case TOUCHSCREEN_DEFAULT:
            tVBOpt.MODIFIED = true;
            tVBOpt.TOUCH_BUTTONS = buttons_on_screen;
            [[gnu::musttail]] return touchscreen_editor();
    }
}

static void init_colour_wheel(int col_int, float *hue_p, float *saturation_p, float *lightness_p) {
    // tint to hue saturation
    float col[3] = {
        (col_int & 0xff) / 255.0,
        ((col_int >> 8) & 0xff) / 255.0,
        ((col_int >> 16) & 0xff) / 255.0,
    };
    float max_rg = col[0] > col[1] ? col[0] : col[1];
    float lightness = max_rg > col[2] ? max_rg : col[2];
    if (lightness_p) *lightness_p = lightness;
    if (lightness != 0) {
        col[0] /= lightness;
        col[1] /= lightness;
        col[2] /= lightness;
    }
    float max = col[0] > col[1] ? col[0] : col[1];
    max = max > col[2] ? max : col[2];
    float min = col[0] < col[1] ? col[0] : col[1];
    min = min < col[2] ? min : col[2];
    if (max == min) {
        // white
        *hue_p = *saturation_p = 0;
    } else {
        float chroma = max - min;
        float hprime;
        if (max == col[0]) {
            hprime = (col[1] - col[2]) / chroma;
        } else if (max == col[1]) {
            hprime = (col[2] - col[0]) / chroma + 2;
        } else {
            hprime = (col[0] - col[1]) / chroma + 4;
        }
        *hue_p = hprime * (M_PI / 3);
        *saturation_p = chroma / max;
    }
}

static int make_color(float hue, float saturation, float lightness) {
    float hprime = fmod(hue / (M_PI / 3) + 6, 6);
    float sub = saturation * (1 - fabs(fmod(hprime, 2) - 1));
    float col[3] = {0};
    if (hprime < 1) {
        col[0] = saturation;
        col[1] = sub;
    } else if (hprime < 2) {
        col[0] = sub;
        col[1] = saturation;
    } else if (hprime < 3) {
        col[1] = saturation;
        col[2] = sub;
    } else if (hprime < 4) {
        col[1] = sub;
        col[2] = saturation;
    } else if (hprime < 5) {
        col[0] = sub;
        col[2] = saturation;
    } else {
        col[0] = saturation;
        col[2] = sub;
    }
    return
        ((int)((col[0] + 1 - saturation) * lightness * 255)) |
        ((int)((col[1] + 1 - saturation) * lightness * 255) << 8) |
        ((int)((col[2] + 1 - saturation) * lightness * 255) << 16);
}

static void col_to_str(char *out, int col) {
    snprintf(out, 8, "#%02x%02x%02x", col & 0xff, (col >> 8) & 0xff, (col >> 16) & 0xff);
}

static SwkbdCallbackResult swkbd_colour_callback(void *user, const char **message, const char *text, size_t text_len) {
    if (text_len != 7) goto fail;
    if (text[0] != '#') goto fail;
    for (int i = 1; i <= 6; i++) {
        if (!(text[i] >= '0' && text[i] <= '9') && !(text[i] >= 'a' && text[i] <= 'f') && !(text[i] >= 'A' && text[i] <= 'F')) goto fail;
    }
    return SWKBD_CALLBACK_OK;
    fail:
    *message = "Please provide a valid hex\ncolor code (# followed by six\nhex digits, eg. #1234ab).";
    return SWKBD_CALLBACK_CONTINUE;
}

static bool swkbd_colour(int *save_col) {
    char string_buf[8];
    col_to_str(string_buf, *save_col);

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(string_buf)-1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
    swkbdSetFilterCallback(&swkbd, swkbd_colour_callback, NULL);

    swkbdSetInitialText(&swkbd, string_buf);

    toggleVsync(false);
    SwkbdButton button = swkbdInputText(&swkbd, string_buf, sizeof(string_buf));
    toggleVsync(tVBOpt.VSYNC);

    if (button == SWKBD_BUTTON_RIGHT) {
        int rev_col = strtol(string_buf + 1, NULL, 16);
        int new_col = (rev_col >> 16) | (rev_col & 0xff00) | ((rev_col & 0xff) << 16);
        if (*save_col != new_col) {
            *save_col = new_col;
            return true;
        }
    }

    return false;
}

static SwkbdCallbackResult swkbd_scale_callback(void *user, const char **message, const char *text, size_t text_len) {
    float scale = atoff(text);
    if (scale < 1 || scale > 4) {
        *message = "Scale must be a number between 1.0 and 4.0.";
        return SWKBD_CALLBACK_CONTINUE;
    }
    return SWKBD_CALLBACK_OK;
}

static bool swkbd_scale(float *scale) {
    char string_buf[8];
    snprintf(string_buf, sizeof(string_buf), "%.5f", *scale);

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, sizeof(string_buf)-1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetNumpadKeys(&swkbd, '.', '.');
    swkbdSetFilterCallback(&swkbd, swkbd_scale_callback, NULL);

    swkbdSetInitialText(&swkbd, string_buf);

    toggleVsync(false);
    SwkbdButton button = swkbdInputText(&swkbd, string_buf, sizeof(string_buf));
    toggleVsync(tVBOpt.VSYNC);

    if (button == SWKBD_BUTTON_RIGHT) {
        *scale = atoff(string_buf);
        return true;
    }
    return false;
}

static void handle_colour_wheel(int *save_col, int wheel_x, int wheel_y, float *hue, float *saturation, float *lightness) {
    static bool dragging_wheel = false;
    static bool dragging_lightness = false;
    C2D_SpriteSetPos(&colour_wheel_sprite, wheel_x, wheel_y);
    const float circle_x = colour_wheel_sprite.params.pos.x;
    const float circle_y = colour_wheel_sprite.params.pos.y;
    const float circle_w = colour_wheel_sprite.params.pos.w;
    const float circle_h = colour_wheel_sprite.params.pos.h;

    const int lightness_x = 16;
    const int lightness_y = 16;
    const int lightness_width = 32;
    const int lightness_height = 180;
    const int lightness_cursor_height = 10;

    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    float touch_dx = (touch_pos.px - circle_x) / (circle_w / 2);
    float touch_dy = (touch_pos.py - circle_y) / (circle_h / 2);
    float sat = sqrt(touch_dx * touch_dx + touch_dy * touch_dy);
    if ((hidKeysDown() & KEY_TOUCH)) {
        if (sat <= 1) dragging_wheel = true;
        if (lightness &&
            (unsigned)(touch_pos.px - (lightness_x - 2)) < lightness_width &&
            abs((int)(touch_pos.py - lightness_y)) < lightness_height
        ) dragging_lightness = true;
    }
    if (hidKeysUp() & KEY_TOUCH) {
        dragging_wheel = false;
        dragging_lightness = false;
    }
    if (dragging_wheel) {
        tVBOpt.MODIFIED = true;
        if (sat > 1) {
            touch_dx /= sat;
            touch_dy /= sat;
            sat = 1;
        }
        *saturation = sat;
        // touch position to hue saturation, then to rgb
        *hue = atan2(touch_dy, touch_dx);
        *save_col = make_color(*hue, *saturation, lightness ? *lightness : 1);
    }
    if (dragging_lightness) {
        *lightness = 1 - C2D_Clamp((float)(touch_pos.py - lightness_y) / lightness_height, 0, 1);
        *save_col = make_color(*hue, *saturation, *lightness);
    }
    C2D_DrawSprite(&colour_wheel_sprite);
    if (!dragging_wheel) {
        touch_dx = *saturation * cos(*hue);
        touch_dy = *saturation * sin(*hue);
    }
    C2D_DrawCircleSolid(
        circle_x + touch_dx * (circle_w / 2),
        circle_y + touch_dy * (circle_h / 2),
        0, 4, 0xff000000);
    C2D_DrawCircleSolid(
        circle_x + touch_dx * (circle_w / 2),
        circle_y + touch_dy * (circle_h / 2),
        0, 2, 0xff000000 | *save_col);
    if (lightness) {
        int max_lightness = 0xff000000 | make_color(*hue, *saturation, 1);
        C2D_DrawRectSolid(lightness_x - 1, lightness_y - 1, 0, lightness_width + 2, lightness_height + 2, 0xff000000 | make_color(*hue + M_PI, *saturation, 1));
        C2D_DrawRectangle(lightness_x, lightness_y, 0, lightness_width, lightness_height, max_lightness, max_lightness, 0xff000000, 0xff000000);
        C2D_DrawRectSolid(lightness_x - 2, lightness_y + lightness_height * (1 - *lightness) - lightness_cursor_height / 2, 0, lightness_width + 4, lightness_cursor_height, 0xffffffff);
        C2D_DrawRectSolid(lightness_x - 1, lightness_y + lightness_height * (1 - *lightness) - lightness_cursor_height / 2 + 1, 0, lightness_width + 2, lightness_cursor_height - 2, 0xff000000);
        C2D_DrawRectSolid(lightness_x, lightness_y + lightness_height * (1 - *lightness) - lightness_cursor_height / 2 + 2, 0, lightness_width, lightness_cursor_height - 4, 0xff000000 | *save_col);
    }
}

static void __attribute__((unused)) colour_filter(void) {
    float hue, saturation, lightness;
    init_colour_wheel(tVBOpt.TINT, &hue, &saturation, NULL);

    LOOP_BEGIN(colour_filter_buttons, -1);
        handle_colour_wheel(&tVBOpt.TINT, 176, 112, &hue, &saturation, NULL);
    LOOP_END(colour_filter_buttons);

    switch (button) {
        case COLOUR_BACK: // Back
            return;
        case COLOUR_RED: // Red
            tVBOpt.TINT = 0x0000ff;
            tVBOpt.MODIFIED = true;
            [[gnu::musttail]] return colour_filter();
        case COLOUR_GRAY: // Gray
            tVBOpt.TINT = 0xffffff;
            tVBOpt.MODIFIED = true;
            [[gnu::musttail]] return colour_filter();
    }
}

static void __attribute__((unused)) multicolour_wheel(int palette_id, int colour_id) {
    float hue, saturation, lightness;
    init_colour_wheel(tVBOpt.MTINT[palette_id][colour_id], &hue, &saturation, &lightness);

    const int scale_x = 272;
    const int scale_y = 40;
    const int scale_width = 32;
    const int scale_height = 160;
    const int scale_cursor_height = 10;
    const int scale_offset = 1;
    const int scale_range = 3;

    bool dragging_scale = 0;

    multicolour_wheel_buttons[MULTIWHEEL_SCALE].hidden = colour_id == 0;

    LOOP_BEGIN(multicolour_wheel_buttons, 0);
        char initial_string[9];
        C2D_TextBufClear(dynamic_textbuf);

        col_to_str(initial_string, tVBOpt.MTINT[palette_id][colour_id]);
        C2D_TextParse(&multicolour_wheel_buttons[MULTIWHEEL_HEX].text, dynamic_textbuf, initial_string);
        C2D_TextOptimize(&multicolour_wheel_buttons[MULTIWHEEL_HEX].text);

        snprintf(initial_string, sizeof(initial_string), "%.5f", tVBOpt.STINT[palette_id][colour_id - 1]);
        C2D_TextParse(&multicolour_wheel_buttons[MULTIWHEEL_SCALE].text, dynamic_textbuf, initial_string);
        C2D_TextOptimize(&multicolour_wheel_buttons[MULTIWHEEL_SCALE].text);

        handle_colour_wheel(&tVBOpt.MTINT[palette_id][colour_id], 160, 112, &hue, &saturation, &lightness);

        if (colour_id != 0) {
            touchPosition touch_pos;
            hidTouchRead(&touch_pos);

            if ((hidKeysDown() & KEY_TOUCH)) {
                if ((unsigned)(touch_pos.px - (scale_x)) < scale_width &&
                    abs((int)(touch_pos.py - scale_y)) < scale_height
                ) dragging_scale = true;
            }
            if (hidKeysUp() & KEY_TOUCH) {
                dragging_scale = false;
            }
            if (dragging_scale) {
                tVBOpt.MODIFIED = true;
                float slider = 1 - C2D_Clamp((float)(touch_pos.py - scale_y) / scale_height, 0, 1);
                tVBOpt.STINT[palette_id][colour_id - 1] = slider * scale_range + scale_offset;
            }

            C2D_DrawText(&text_brighten, C2D_AlignCenter | C2D_WithColor, scale_x + scale_width / 2, scale_y - 24, 0, 0.5, 0.5, TINT_COLOR);
            C2D_DrawText(&text_brightness_disclaimer, C2D_AlignRight | C2D_WithColor, 316, 0, 0, 0.5, 0.5, TINT_COLOR);
            C2D_DrawRectSolid(scale_x + scale_width / 2 - 1, scale_y, 0, 2, scale_height, 0xff404040);
            C2D_DrawRectSolid(scale_x, scale_y + scale_height * (1 - (tVBOpt.STINT[palette_id][colour_id - 1] - scale_offset) / scale_range) - scale_cursor_height / 2, 0, scale_width, scale_cursor_height, TINT_COLOR);
        }

    LOOP_END(multicolour_wheel_buttons);

    switch (button) {
        case MULTIWHEEL_BACK:
            [[gnu::musttail]] return multicolour_settings(palette_id, colour_id);
        case MULTIWHEEL_HEX:
            if (swkbd_colour(&tVBOpt.MTINT[palette_id][colour_id])) tVBOpt.MODIFIED = true;
            [[gnu::musttail]] return multicolour_wheel(palette_id, colour_id);
        case MULTIWHEEL_SCALE:
            if (swkbd_scale(&tVBOpt.STINT[palette_id][colour_id - 1])) tVBOpt.MODIFIED = true;
            [[gnu::musttail]] return multicolour_wheel(palette_id, colour_id);
    }
}

static void multicolour_picker(int initial_button) {
    int focus=0;
    for(;;) {
        const char *labels[]={"Palette 1","Palette 2","Palette 3","Palette 4"};
        int b=rvc_list("LEGACY PALETTES",labels,4,focus,"Back",NULL);
        if(b<0 || b==4) {return;}
        focus=b;tVBOpt.MULTIID=b;tVBOpt.MODIFIED=true;
        multicolour_settings(b,0);
    }
}

static void multicolour_settings(int palette_id, int initial_button) {
    int focus=0;
    for(;;) {
        const char *labels[]={"Darkest Color","Dark Color","Light Color","Lightest Color","Brightness Levels"};
        int b=rvc_list("PALETTE SHADES",labels,5,focus,"Back",NULL);
        if(b<0 || b==5) {return;}
        focus=b;
        if(b<4) {if(swkbd_colour(&tVBOpt.MTINT[palette_id][b])) tVBOpt.MODIFIED=true;}
        else {
            const char *brightness[]={"Dark Brightness","Light Brightness","Lightest Brightness"};
            int c=rvc_list("BRIGHTNESS",brightness,3,0,"Back",NULL);
            if(c>=0 && c<3 && swkbd_scale(&tVBOpt.STINT[palette_id][c])) tVBOpt.MODIFIED=true;
        }
    }
}

static bool vblink_transfer() {
    C2D_Text fname;
    C2D_TextBufClear(dynamic_textbuf);
    C2D_TextParse(&fname, dynamic_textbuf, vblink_fname);
    C2D_TextOptimize(&fname);
    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN false
    LOOP_BEGIN(load_rom_buttons, -1);
        rvc_ui_frame("VBLINK TRANSFER");
        C2D_DrawText(&fname, C2D_AlignCenter | C2D_WithColor, 320 / 2, 54, 0, 0.5, 0.5, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
        C2D_DrawText(&text_loading, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.8, 0.8, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
        C2D_DrawRectSolid(60, 140, 0, 200, 16, C2D_Color32(0.5 * TINT_R, 0.5 * TINT_G, 0.5 * TINT_B, 255));
        C2D_DrawRectSolid(60, 140, 0, 2 * vblink_progress, 16, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
        if (vblink_progress == 100) {
            // complete, all good
            svcSignalEvent(vblink_event);
            guiop = AKILL | VBRESET;
            game_running = true;
            loadGameOptions();
            last_savestate = 0;
            loop = false;
        } else if (vblink_progress < 0 || vblink_error) {
            vblink_error = 0;
            svcSignalEvent(vblink_event);
            loop = false;
        }
    LOOP_END(load_rom_buttons);
    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN
    if (vblink_progress != 100) {
        game_running = false;
        if (tVBOpt.GAME_SETTINGS) loadFileOptions();
        // redraw logo since we unloaded
        C3D_FrameBegin(0);
        draw_logo();
        C3D_FrameEnd(0);
    }
    return vblink_progress == 100;
}

static void vblink(void) {
    bool loaded = false;
    char str[100];
    C2D_Text text;
    int open_err = vblink_open();
    union {
        struct in_addr ia;
        u8 bytes[4];
    } ip_addr, netmask, broadcast;
    LOOP_BEGIN(about_buttons, 0);
        if(!game_running) draw_logo();
        rvc_ui_frame("VBLINK TRANSFER");
        SOCU_GetIPInfo(&ip_addr.ia, &netmask.ia, &broadcast.ia);
        C2D_TextBufClear(dynamic_textbuf);
        if (open_err) {
            snprintf(str, sizeof(str), "Couldn't start: error %d\nPlease ensure Wi-Fi is enabled and try again.", open_err);
        } else if (vblink_progress == -2) {
            snprintf(str, sizeof(str), "Socket closed: error %d\nPlease ensure Wi-Fi is enabled and try again.", vblink_error);
        } else if (vblink_error) {
            snprintf(str, sizeof(str), "Listening at %d.%d.%d.%d...\nTransfer failed: error %d", ip_addr.bytes[0], ip_addr.bytes[1], ip_addr.bytes[2], ip_addr.bytes[3], vblink_error);
        } else {
            snprintf(str, sizeof(str), "Listening at %d.%d.%d.%d...", ip_addr.bytes[0], ip_addr.bytes[1], ip_addr.bytes[2], ip_addr.bytes[3]);
        }
        C2D_TextParse(&text, dynamic_textbuf, str);
        C2D_TextOptimize(&text);
        rvc_ui_text(str,160,86,true);
        if (vblink_progress == 0) loop = false;
    LOOP_END(about_buttons);
    if (vblink_progress == 0) {
        // ready to receive
        svcSignalEvent(vblink_event);
        bool success = vblink_transfer();
        vblink_close();
        if (success) {
            // success
            return;
        } else [[gnu::musttail]] return vblink();
    } else {
        vblink_close();
        [[gnu::musttail]] return main_menu(game_running ? MAIN_MENU_RESUME : MAIN_MENU_LOAD_ROM);
    }
}

static void dev_options(int initial_button) {
    bool newer=false; APT_CheckNew3DS(&newer);
    int focus=0;
    for(;;) {
        const char *labels[]={tVBOpt.PERF_INFO?"Status Bar: On":"Status Bar: Off",tVBOpt.VIP_OVERCLOCK?"VIP Overclock: On":"VIP Overclock: Off",tVBOpt.N3DS_SPEEDUP?"New 3DS Speedup: On":"New 3DS Speedup: Off"};
        int count=newer?3:2;
        int b=rvc_list("PERFORMANCE",labels,count,focus,"Back",NULL);
        if(b<0 || b==count) return;
        focus=b; tVBOpt.MODIFIED=true;
        if(b==0) tVBOpt.PERF_INFO=!tVBOpt.PERF_INFO;
        if(b==1) tVBOpt.VIP_OVERCLOCK=!tVBOpt.VIP_OVERCLOCK;
        if(b==2) { tVBOpt.N3DS_SPEEDUP=!tVBOpt.N3DS_SPEEDUP; osSetSpeedupEnable(tVBOpt.N3DS_SPEEDUP); }
    }
}

static void options(int initial_button) {
    rvc_settings();
}

static void video_settings(int initial_button) {
    int focus=0;
    for(;;) {
        const char *labels[]={tVBOpt.ANAGLYPH?"3D Mode: Anaglyph":"3D Mode: Nintendo 3DS","Configure 3D",tVBOpt.ANTIFLICKER?"Antiflicker: On":"Antiflicker: Off",tVBOpt.SLIDERMODE?"Slider: Virtual Boy IPD":"Slider: Nintendo 3DS"};
        int count=any_2ds?3:4;
        int b=rvc_list("DISPLAY / 3D",labels,count,focus,"Back",NULL);
        if(b<0 || b==count) return;
        focus=b;
        if(b==0) { toggleAnaglyph(!tVBOpt.ANAGLYPH); tVBOpt.MODIFIED=true; }
        if(b==1) { if(tVBOpt.ANAGLYPH) anaglyph_settings(0); else barrier_settings(0); }
        if(b==2) { tVBOpt.ANTIFLICKER=!tVBOpt.ANTIFLICKER; tVBOpt.MODIFIED=true; }
        if(b==3) { tVBOpt.SLIDERMODE=!tVBOpt.SLIDERMODE; tVBOpt.MODIFIED=true; }
        if(guiop & GUIEXIT) return;
    }
}

static void barrier_settings(int initial_button) {
    int focus=0;
    for(;;) {
        const char *labels[]={tVBOpt.MULTICOL?"Legacy Palette: On":"Legacy Palette: Off","Configure Palette",tVBOpt.DEFAULT_EYE?"Default Eye: Right":"Default Eye: Left"};
        int b=rvc_list("STEREO SETTINGS",labels,3,focus,"Back",NULL);
        if(b<0 || b==3) return;
        focus=b;
        if(b==0) { tVBOpt.MULTICOL=!tVBOpt.MULTICOL; tVBOpt.MODIFIED=true; }
        if(b==1) { if(tVBOpt.MULTICOL) multicolour_picker(0); else rvc_pick_color(); }
        if(b==2) { tVBOpt.DEFAULT_EYE=!tVBOpt.DEFAULT_EYE; tVBOpt.MODIFIED=true; }
    }
}

static void anaglyph_settings(int initial_button) {
    int focus=0;
    for(;;) {
        char left[48],right[48],depth[48];
        const char *filters[]={"Off","Red","Green","Yellow","Blue","Magenta","Cyan","White"};
        snprintf(left,48,"Left Filter: %s",filters[tVBOpt.ANAGLYPH_LEFT]);snprintf(right,48,"Right Filter: %s",filters[tVBOpt.ANAGLYPH_RIGHT]);snprintf(depth,48,"Depth: %d",tVBOpt.ANAGLYPH_DEPTH);
        const char *labels[]={left,right,depth};int count=any_2ds?3:2;
        int b=rvc_list("ANAGLYPH",labels,count,focus,"Back",NULL);
        if(b<0 || b==count) return;
        focus=b;tVBOpt.MODIFIED=true;
        if(b==0) {tVBOpt.ANAGLYPH_LEFT=(tVBOpt.ANAGLYPH_LEFT+1)%8;tVBOpt.ANAGLYPH_RIGHT&=~tVBOpt.ANAGLYPH_LEFT;}
        if(b==1) {tVBOpt.ANAGLYPH_RIGHT=(tVBOpt.ANAGLYPH_RIGHT+1)%8;tVBOpt.ANAGLYPH_LEFT&=~tVBOpt.ANAGLYPH_RIGHT;}
        if(b==2) tVBOpt.ANAGLYPH_DEPTH=tVBOpt.ANAGLYPH_DEPTH==8?-8:tVBOpt.ANAGLYPH_DEPTH+1;
    }
}

static void sound_error(void) {
    LOOP_BEGIN(sound_error_buttons, 0);
        rvc_ui_frame("AUDIO UNAVAILABLE");
        rvc_ui_text("COULD NOT INITIALIZE AUDIO",160,76,true);
        rvc_ui_text("CHECK YOUR DSP FIRMWARE",160,96,true);
    LOOP_END(sound_error_buttons);
    return;
}

static bool areyousure(C2D_Text *message) {
    const char *title=message==&text_areyousure_reset?"RESTART GAME?":message==&text_areyousure_exit?"EXIT EMULATOR?":"LEAVE MULTIPLAYER?";
    const char *labels[]={"Yes","No"};
    return rvc_list(title,labels,2,1,"Back",NULL)==0;
}

static void __attribute__((unused)) savestate_error(char *message, int last_button, int selected_state) {
    C2D_Text text;
    C2D_TextBufClear(dynamic_textbuf);
    C2D_TextParse(&text, dynamic_textbuf, message);
    C2D_TextOptimize(&text);
    LOOP_BEGIN(about_buttons, 0);
        C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.7, 0.7, TINT_COLOR);
    LOOP_END(about_buttons);
    [[gnu::musttail]] return savestate_menu(last_button, selected_state);
}

static void __attribute__((unused)) savestate_confirm(char *message, int last_button, int selected_state) {
    C2D_Text text;
    C2D_TextBufClear(dynamic_textbuf);
    C2D_TextParse(&text, dynamic_textbuf, message);
    C2D_TextOptimize(&text);
    LOOP_BEGIN(savestate_confirm_buttons, 0);
        C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.7, 0.7, TINT_COLOR);
    LOOP_END(savestate_confirm_buttons);
    if (button) [[gnu::musttail]] return savestate_menu(last_button, selected_state);
    else return;
}

static void savestate_menu(int initial_button, int selected_state) {
    int focus=0;
    for(;;) {
        char title[32]; snprintf(title,sizeof(title),"SAVE / LOAD - SLOT %d",selected_state);
        bool exists=emulation_hasstate(selected_state);
        const char *labels[]={"Save State",exists?"Load State":"Load State (Empty)","Previous Slot","Next Slot",exists?"Delete State":"Delete State (Empty)"};
        int b=rvc_list(title,labels,5,focus,"Back",exists?"SLOT OCCUPIED":"SLOT EMPTY");
        if(b<0 || b==5) return;
        focus=b; last_savestate=selected_state;
        if(b==2 || b==3) { selected_state=(selected_state+(b==2?9:1))%10; last_savestate=selected_state; continue; }
        if((b==1 || b==4) && !exists) { rvc_message("EMPTY SLOT","NO STATE IN THIS SLOT","CHOOSE ANOTHER SLOT"); continue; }
        if(b==0 && exists) { const char *q[]={"Replace State","Keep Existing State"}; if(rvc_list("REPLACE THIS STATE?",q,2,1,"Back",NULL)!=0) continue; }
        if(b==4) { const char *q[]={"Delete State","Keep State"}; if(rvc_list("DELETE THIS STATE?",q,2,1,"Back",NULL)!=0) continue; }
        int result=b==0?emulation_sstate(selected_state):b==1?emulation_lstate(selected_state):emulation_rmstate(selected_state);
        if(result) { rvc_message("STATE ERROR","OPERATION FAILED","CHECK SD CARD AND FREE SPACE"); continue; }
        rvc_message(b==0?"STATE SAVED":b==1?"STATE LOADED":"STATE DELETED","OPERATION COMPLETE","BACK RETURNS TO SAVE / LOAD");
    }
}

static void __attribute__((unused)) about(void) {
    C2D_SpriteSetPos(&logo_sprite, 320 / 2, 36);
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32(255, 0, 0, 255), 1);
    LOOP_BEGIN(about_buttons, 0);
        C2D_DrawSpriteTinted(&logo_sprite, &tint);
        C2D_DrawText(&text_about, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.5, 0.5, TINT_COLOR);
    LOOP_END(about_buttons);
    [[gnu::musttail]] return options(OPTIONS_ABOUT);
}

static bool load_error(int err, bool unloaded, char *rom_message) {
    C2D_Text text;
    char code_message[32];
    snprintf(code_message, sizeof(code_message), "Error code: %d", err);
    C2D_TextParse(&text, dynamic_textbuf, code_message);
    C2D_TextOptimize(&text);
    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN false
    LOOP_BEGIN(about_buttons, 0);
        C2D_DrawText(&text_loaderr, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 120, 0, 0.5, 0.5, TINT_COLOR);
        if (unloaded) {
            C2D_DrawText(&text_unloaded, C2D_AlignCenter | C2D_WithColor, 320 / 2, 160, 0, 0.5, 0.5, TINT_COLOR);
        }
    LOOP_END(about_buttons);
    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN
    [[gnu::musttail]] return rom_loader(rom_message);
}

static void forwarder_error(int err) {
    C2D_Text text;
    char code_message[32];
    snprintf(code_message, sizeof(code_message), "Error code: %d", err);
    C2D_TextParse(&text, dynamic_textbuf, code_message);
    C2D_TextOptimize(&text);
    LOOP_BEGIN(forwarder_error_buttons, 0);
        C2D_DrawText(&text_loaderr, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.5, 0.5, TINT_COLOR);
        C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 120, 0, 0.5, 0.5, TINT_COLOR);
    LOOP_END(forwarder_error_buttons);
    guiop = GUIEXIT;
    return;
}

static bool load_rom(char *rom_message) {
    if (save_thread) threadJoin(save_thread, U64_MAX);
    int ret;
    if ((ret = v810_load_init())) {
        // instant fail
        if (tVBOpt.FORWARDER) {
            forwarder_error(ret);
            return false;
        } else {
            [[gnu::musttail]] return load_error(ret, false, rom_message);
        }
    }
    C2D_Text text;
    C2D_TextBufClear(dynamic_textbuf);
    char *filename = strrchr(tVBOpt.ROM_PATH, '/');
    if (filename) filename++;
    else filename = tVBOpt.ROM_PATH;
    C2D_TextParse(&text, dynamic_textbuf, filename);
    C2D_TextOptimize(&text);

    load_rom_buttons[0].hidden = tVBOpt.FORWARDER;

    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN false
    LOOP_BEGIN(load_rom_buttons, -1);
        ret = v810_load_step();
        if (ret < 0) {
            // error
            loop = false;
        } else {
            rvc_ui_frame("LOADING ROM");
            C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor,160,72,0,.4,.4,0xffe8e8e8);
            rvc_ui_text("LOADING...",160,110,true);
            C2D_DrawRectSolid(60, 140, 0, 200, 16, TINT_50);
            C2D_DrawRectSolid(60, 140, 0, 2 * ret, 16, TINT_COLOR);
            if (ret == 100) {
                // complete
                loop = false;
            }
        }
    LOOP_END(load_rom_buttons);
    #undef DEFAULT_RETURN
    #define DEFAULT_RETURN
    if (tVBOpt.GAME_SETTINGS) loadFileOptions();
    if (ret == 100) {
        // complete
        game_running = true;
        loadGameOptions();
        last_savestate = 0;
        return true;
    } else {
        game_running = false;
        // redraw logo since we unloaded
        C3D_FrameBegin(0);
        draw_logo();
        C3D_FrameEnd(0);
        if (ret < 0) {
            // error
            if (tVBOpt.FORWARDER) {
                forwarder_error(ret);
                return false;
            } else {
                [[gnu::musttail]] return load_error(ret, true, rom_message);
            }
        } else {
            // cancelled
            v810_load_cancel();
            [[gnu::musttail]] return rom_loader(rom_message);
        }
    }
}

static inline int handle_buttons(Button buttons[], int count) {
    static int pressed = -1;
    int ret = -1;
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);

    // d-pad input
    u32 kDown = hidKeysDown();
    if ((kDown & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) && !buttonLock)
    {
        // if no button is selected just use the first one
        if (selectedButton == NULL)
            selectedButton = &buttons[0];
        else
        {
            // search for button nearest to the selected one
            Button* temp = NULL;
            int xaxis = (bool)(kDown & KEY_RIGHT) - (bool)(kDown & KEY_LEFT);
            int yaxis = (bool)(kDown & KEY_DOWN) - (bool)(kDown & KEY_UP);
            int selectx = (selectedButton->x + (selectedButton->w / 2)) + ((selectedButton->w / 2) * xaxis);
            int selecty = (selectedButton->y + (selectedButton->h / 2)) + ((selectedButton->h / 2) * yaxis);

            for (int i = 0; i < count; i++)
            {
                // skip if selected button
                if (selectedButton == &buttons[i])
                    continue;

                // skip if hidden
                if (buttons[i].hidden)
                    continue;

                // skip if button is not in the right direction
                int buttonx = (buttons[i].x + (buttons[i].w / 2)) - ((buttons[i].w / 2) * xaxis);
                int buttony = (buttons[i].y + (buttons[i].h / 2)) - ((buttons[i].h / 2) * yaxis);

                if (((kDown & KEY_UP)    && buttony >= selecty) ||
                    ((kDown & KEY_DOWN)  && buttony <= selecty) ||
                    ((kDown & KEY_LEFT)  && buttonx >= selectx) ||
                    ((kDown & KEY_RIGHT) && buttonx <= selectx))
                {
                    continue;
                }

                // use the nearest button that is in the right direction
                if (temp) {
                    int tempx = (temp->x + (temp->w / 2)) - ((temp->w / 2) * xaxis);
                    int tempy = (temp->y + (temp->h / 2)) - ((temp->h / 2) * yaxis);
                    int olddist = dis(selectx, selecty, tempx, tempy);
                    int newdist = dis(selectx, selecty, buttonx, buttony);
                    if (newdist > olddist)
                        continue;
                    // bias leftwards
                    if (olddist == newdist && buttonx > tempx)
                        continue;
                }

                temp = &buttons[i];
            }

            if (temp)
                selectedButton = temp;
        }
    }

    // check buttons for input
    for (int i = 0; i < count; i++) {
        if (buttons[i].hidden || buttons[i].disabled) continue;
        if (
            touch_pos.px >= buttons[i].x && touch_pos.py >= buttons[i].y &&
            touch_pos.px < buttons[i].x + buttons[i].w && touch_pos.py < buttons[i].y + buttons[i].h
        ) {
            // touching the button
            if (hidKeysDown() & KEY_TOUCH) {
                pressed = i;
                selectedButton = &buttons[i];
            }
        } else {
            // not touching the button
            if (pressed == i && (hidKeysHeld() & KEY_TOUCH)) {
                pressed = -1;
            }
        }

        if ((hidKeysUp() & KEY_TOUCH) && pressed == i) {
            ret = i;
        }

        // select with the A button
        if ((kDown & KEY_A) && selectedButton == &buttons[i] && !buttonLock) {
            ret = i;
        }

        // text-based checks, ignore when there is not text
        if (!buttons[i].str) continue;

        // move back with the B button
        if ((kDown & KEY_B) && (strcmp(buttons[i].str, "Back") == 0 || strcmp(buttons[i].str, "No") == 0 || strcmp(buttons[i].str, "Cancel") == 0)) {
            ret = i;
        }

        // move up a directory with X
        if ((kDown & KEY_X) && (strcmp(buttons[i].str, "Up") == 0 || strcmp(buttons[i].str, "Parent Folder") == 0)) {
            ret = i;
        }
    }
    // draw buttons
    for (int i = 0; i < count; i++) {
        if (buttons[i].hidden) continue;
        rvc_ui_patch(buttons[i].x,buttons[i].y,buttons[i].w,buttons[i].h,
            buttons[i].disabled ? 3 : pressed == i ? 2 : selectedButton == &buttons[i] ? 1 : buttons[i].secondary ? 3 : 0);
        if(buttons[i].icon_plus_one) { int size=buttons[i].h<22?14:18; rvc_ui_image(buttons[i].icon_plus_one-1,buttons[i].x+8,buttons[i].y+floorf((buttons[i].h-size)/2),size,size); }
        if (buttons[i].custom_draw) buttons[i].custom_draw(&buttons[i]);
        else if (buttons[i].str) {
            if (strstr(buttons[i].str,"\xEE")) {
                C2D_DrawText(&buttons[i].text,C2D_AlignCenter|C2D_WithColor,buttons[i].x+buttons[i].w/2,buttons[i].y+buttons[i].h/2-10,0,.6,.6,TINT_COLOR);
            } else {
                rvc_ui_label(buttons[i].str,buttons[i].x,buttons[i].y,buttons[i].w,buttons[i].h);
            }
        }
        if (buttons[i].show_toggle) C2D_DrawText(buttons[i].toggle ? buttons[i].toggle_text_on : buttons[i].toggle_text_off,
            C2D_WithColor,buttons[i].x+6,buttons[i].y+2,0,.4,.4,TINT_COLOR);
        if (buttons[i].show_option) C2D_DrawText(buttons[i].option_texts[buttons[i].option],C2D_WithColor,buttons[i].x+6,buttons[i].y+2,0,.4,.4,TINT_COLOR);
    }
    if (save_thread) C2D_DrawText(&text_saving, C2D_AlignLeft, 0, 224, 0, 0.5, 0.5);
    if (ret >= 0) pressed = -1;
    return ret;
}

static void draw_status_bar(float total_time, float drc_time) {
    if (!tVBOpt.PERF_INFO) return;
    C2D_DrawRectSolid(0, 240 - 12, 0, 320, 12, C2D_Color32(128, 128, 128, 255));
    C2D_TextBufClear(dynamic_textbuf);
    C2D_Text text;
    char buf[32];

    // All
    sprintf(buf, "A:%5.2fms", total_time);
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 325-53*6, 240 - 12, 0, 0.35, 0.35);

    // DRC
    sprintf(buf, "D:%5.2fms", drc_time);
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 325-53*5, 240 - 12, 0, 0.35, 0.35);

    // C3D
    sprintf(buf, "C:%5.2fms", C3D_GetProcessingTime());
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 325-53*4, 240 - 12, 0, 0.35, 0.35);

    // PICA
    sprintf(buf, "P:%5.2fms", C3D_GetDrawingTime());
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 325-53*3, 240 - 12, 0, 0.35, 0.35);

    // Memory
    sprintf(buf, "M:%5.2f%%", (cache_pos-cache_start)*4*100./CACHE_SIZE);
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 325-53*2, 240 - 12, 0, 0.35, 0.35);

    // VIP
    sprintf(buf, "V:%d", videoProcessingTime());
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 330-54-7, 240 - 12, 0, 0.35, 0.35);


}

void guiInit(void) {
    u8 model = 0;
    cfguInit();
    CFGU_GetSystemModel(&model);
    cfguExit();
    old_2ds = (model == CFG_MODEL_2DS);
    any_2ds = old_2ds || model == CFG_MODEL_N2DSXL;

    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

    screen = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, GPU_RB_RGB8, -1);
    C3D_RenderTargetSetOutput(screen, GFX_BOTTOM, GFX_LEFT,
			GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
			GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    C2D_SetTintMode(C2D_TintMult);

    sprite_sheet = C2D_SpriteSheetLoadFromMem(sprites_t3x, sprites_t3x_size);
    C2D_SpriteFromSheet(&colour_wheel_sprite, sprite_sheet, sprites_colour_wheel_idx);
    C2D_SpriteSetCenter(&colour_wheel_sprite, 0.5, 0.5);
    C2D_SpriteFromSheet(&logo_sprite, sprite_sheet, sprites_logo_idx);
    C2D_SpriteSetCenter(&logo_sprite, 0.5, 0.5);
    C2D_SpriteFromSheet(&text_3ds_sprite, sprite_sheet, sprites_3ds_text_idx);
    C2D_SpriteSetPos(&text_3ds_sprite, 128, 18);
    C2D_SpriteFromSheet(&text_vb_sprite, sprite_sheet, sprites_vb_text_idx);
    C2D_SpriteSetPos(&text_vb_sprite, 178, 98);
    C2D_SpriteFromSheet(&vb_icon_sprite, sprite_sheet, sprites_vb_icon_idx);
    C2D_SpriteSetPos(&vb_icon_sprite, 137, 120);
    C2D_SpriteFromSheet(&text_toggle_sprite, sprite_sheet, sprites_toggle_text_idx);
    C2D_SpriteFromSheet(&text_turbo_sprite, sprite_sheet, sprites_turbo_text_idx);

    splash_sheet = C2D_SpriteSheetLoadFromMem(splash_t3x, splash_t3x_size);
    C2D_SpriteFromSheet(&splash_left, splash_sheet, splash_splash_left_idx);
    C2D_SpriteFromSheet(&splash_right, splash_sheet, splash_splash_right_idx);
    C2D_SpriteSetPos(&splash_right, 0, 256);

    if(tVBOpt.TOUCH_AX==259 && tVBOpt.TOUCH_AY==110 && tVBOpt.TOUCH_BX==194 && tVBOpt.TOUCH_BY==152) { tVBOpt.TOUCH_AY=126;tVBOpt.TOUCH_BY=168; }
    if(tVBOpt.TOUCH_AX==250 && tVBOpt.TOUCH_AY==64 && tVBOpt.TOUCH_BX==250 && tVBOpt.TOUCH_BY==160 && tVBOpt.TOUCH_PADX==240 && tVBOpt.TOUCH_PADY==128) {
        tVBOpt.PAUSE_RIGHT=152;tVBOpt.TOUCH_AX=259;tVBOpt.TOUCH_AY=126;
        tVBOpt.TOUCH_BX=194;tVBOpt.TOUCH_BY=168;tVBOpt.TOUCH_PADX=224;tVBOpt.TOUCH_PADY=130;
    }
    rvc_ui_init();
    rvc_ui_theme(tVBOpt.TINT);
    static_textbuf = C2D_TextBufNew(4096);
    dynamic_textbuf = C2D_TextBufNew(4096);
    STATIC_TEXT(&text_A, "A")
    STATIC_TEXT(&text_B, "B")
    STATIC_TEXT(&text_btn_A, "\uE000")
    STATIC_TEXT(&text_btn_B, "\uE001")
    STATIC_TEXT(&text_btn_X, "\uE002")
    STATIC_TEXT(&text_btn_L, "\uE004")
    STATIC_TEXT(&text_btn_R, "\uE005")
    STATIC_TEXT(&text_switch, "Switch")
    STATIC_TEXT(&text_saving, "Saving...")
    STATIC_TEXT(&text_on, "On")
    STATIC_TEXT(&text_off, "Off")
    STATIC_TEXT(&text_toggle, "Toggle")
    STATIC_TEXT(&text_hold, "Hold")
    STATIC_TEXT(&text_nintendo_3ds, "Nintendo 3DS")
    STATIC_TEXT(&text_vbipd, "Virtual Boy IPD")
    STATIC_TEXT(&text_vb_lpad, "Virtual Boy Left D-Pad")
    STATIC_TEXT(&text_vb_rpad, "Virtual Boy Right D-Pad")
    STATIC_TEXT(&text_mirror_abxy, "Mirror ABXY Buttons")
    STATIC_TEXT(&text_vblink, "VBLink")
    STATIC_TEXT(&text_left, "Left")
    STATIC_TEXT(&text_right, "Right")
    STATIC_TEXT(&text_sound_error, "Error: couldn't initialize audio.\nDid you dump your DSP firmware?")
    STATIC_TEXT(&text_debug_filenames, "Please share debug_info.txt and\ndebug_replay.bin.gz in your bug\nreport.")
    STATIC_TEXT(&text_anykeyexit, "Press any key to exit")
    STATIC_TEXT(&text_about, VERSION "\nBy Floogle, danielps, & others\nSplash screen by Morintari\nCustom control scheme by nevumx\nHeavily based on Reality Boy by David Tucker\nMore info at: github.com/skyfloogle/red-viper")
    STATIC_TEXT(&text_loading, "Loading...")
    STATIC_TEXT(&text_loaderr, "Failed to load ROM.")
    STATIC_TEXT(&text_unloaded, "The current ROM has been unloaded.")
    STATIC_TEXT(&text_yes, "Yes")
    STATIC_TEXT(&text_no, "No")
    STATIC_TEXT(&text_areyousure_reset, "Are you sure you want to reset?")
    STATIC_TEXT(&text_areyousure_exit, "Are you sure you want to exit?")
    STATIC_TEXT(&text_areyousure_leave, "Are you sure you want to\nexit multiplayer?")
    STATIC_TEXT(&text_version_mismatch, "Your emulator version doesn't\nmatch the host's.\nThis can cause desyncs.")
    STATIC_TEXT(&text_your_version, "Your version: " VERSION)
    STATIC_TEXT(&text_connect_anyway, "Connect anyway?")
    STATIC_TEXT(&text_protocol_mismatch, "Your emulator version doesn't\nmatch the host's.\nPlease ensure both are updated.\nYour version: " VERSION)
    STATIC_TEXT(&text_savestate_menu, "Savestates")
    STATIC_TEXT(&text_save, "Save")
    STATIC_TEXT(&text_load, "Load")
    STATIC_TEXT(&text_preset, "Preset")
    STATIC_TEXT(&text_custom, "Custom")
    STATIC_TEXT(&text_custom_3ds_button_DUP, "\uE079")
    STATIC_TEXT(&text_custom_3ds_button_DDOWN, "\uE07A")
    STATIC_TEXT(&text_custom_3ds_button_DLEFT, "\uE07B")
    STATIC_TEXT(&text_custom_3ds_button_DRIGHT, "\uE07C")
    STATIC_TEXT(&text_custom_3ds_button_CPAD_UP, "\uE077\uE01B")
    STATIC_TEXT(&text_custom_3ds_button_CPAD_DOWN, "\uE077\uE01C")
    STATIC_TEXT(&text_custom_3ds_button_CPAD_LEFT, "\uE077\uE01A")
    STATIC_TEXT(&text_custom_3ds_button_CPAD_RIGHT, "\uE077\uE019")
    STATIC_TEXT(&text_custom_3ds_button_CSTICK_UP, "\uE04A\uE01B")
    STATIC_TEXT(&text_custom_3ds_button_CSTICK_DOWN, "\uE04A\uE01C")
    STATIC_TEXT(&text_custom_3ds_button_CSTICK_LEFT, "\uE04A\uE01A")
    STATIC_TEXT(&text_custom_3ds_button_CSTICK_RIGHT, "\uE04A\uE019")
    STATIC_TEXT(&text_custom_3ds_button_A, "\uE000")
    STATIC_TEXT(&text_custom_3ds_button_X, "\uE002")
    STATIC_TEXT(&text_custom_3ds_button_B, "\uE001")
    STATIC_TEXT(&text_custom_3ds_button_Y, "\uE003")
    STATIC_TEXT(&text_custom_3ds_button_START, "STA")
    STATIC_TEXT(&text_custom_3ds_button_SELECT, "SEL")
    STATIC_TEXT(&text_custom_3ds_button_L, "\uE004")
    STATIC_TEXT(&text_custom_3ds_button_R, "\uE005")
    STATIC_TEXT(&text_custom_3ds_button_ZL, "\uE054")
    STATIC_TEXT(&text_custom_3ds_button_ZR, "\uE055")
    STATIC_TEXT(&text_custom_vb_button_KEY_L, "\uE004")
    STATIC_TEXT(&text_custom_vb_button_KEY_R, "\uE005")
    STATIC_TEXT(&text_custom_vb_button_KEY_SELECT, "SEL")
    STATIC_TEXT(&text_custom_vb_button_KEY_START, "STA")
    STATIC_TEXT(&text_custom_vb_button_KEY_B, "\uE001")
    STATIC_TEXT(&text_custom_vb_button_KEY_A, "\uE000")
    STATIC_TEXT(&text_custom_vb_button_RPAD_R, "R\uE07C")
    STATIC_TEXT(&text_custom_vb_button_RPAD_L, "R\uE07B")
    STATIC_TEXT(&text_custom_vb_button_RPAD_D, "R\uE07A")
    STATIC_TEXT(&text_custom_vb_button_RPAD_U, "R\uE079")
    STATIC_TEXT(&text_custom_vb_button_LPAD_R, "L\uE07C")
    STATIC_TEXT(&text_custom_vb_button_LPAD_L, "L\uE07B")
    STATIC_TEXT(&text_custom_vb_button_LPAD_D, "L\uE07A")
    STATIC_TEXT(&text_custom_vb_button_LPAD_U, "L\uE079")
    STATIC_TEXT(&text_error, "Error")
    STATIC_TEXT(&text_3ds, "3DS")
    STATIC_TEXT(&text_vb, "VB")
    STATIC_TEXT(&text_map, "MAP")
    STATIC_TEXT(&text_currently_mapped_to, "Currently\nmapped to:")
    STATIC_TEXT(&text_normal, "Normal")
    STATIC_TEXT(&text_turbo, "Turbo")
    STATIC_TEXT(&text_current_default, "Current mode is default")
    STATIC_TEXT(&text_anaglyph, "Anaglyph")
    STATIC_TEXT(&text_depth, "Depth")
    STATIC_TEXT(&text_cpp_on, "Circle Pad Pro connected.")
    STATIC_TEXT(&text_cpp_off, "No Circle Pad Pro found.")
    STATIC_TEXT(&text_monochrome, "Monochrome")
    STATIC_TEXT(&text_multicolor, "Multicolor")
    STATIC_TEXT(&text_brighten, "Brighten")
    STATIC_TEXT(&text_brightness_disclaimer, "Actual brightness may vary by game.")
    STATIC_TEXT(&text_multi_waiting, "Waiting for connection...")
    STATIC_TEXT(&text_multi_preparing, "Peer is preparing...")
    STATIC_TEXT(&text_downloading, "Downloading...")
    STATIC_TEXT(&text_multi_init_error, "Could not start wireless.\nIs wireless enabled?")
    STATIC_TEXT(&text_multi_disconnect, "Disconnected.")
    STATIC_TEXT(&text_multi_comm_error, "Communication error.")
    STATIC_TEXT(&text_multi_reset_on_join, "Game will reset when connecting.")
    STATIC_TEXT(&text_input_buffer, "Input buffer:")
    STATIC_TEXT(&text_multi_no_match, "Loaded game does not match.")
    STATIC_TEXT(&text_multi_not_loaded, "No game is currently loaded.")
    STATIC_TEXT(&text_dlplay_saving, "Note: Download Play will disable saving.")
    STATIC_TEXT(&text_forwarder_nomatch, "Forwarders cannot join other games.")
    SETUP_ALL_BUTTONS
}

bool backlightEnabled = true;

bool toggleBacklight(bool enable) {
    gspLcdInit();
    enable ? GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM) : GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
    gspLcdExit();
    return enable;
}

static bool shouldRedrawMenu = true;
static bool inMenu = false;

void openMenu(bool my_menu) {
    bool oldBacklight = backlightEnabled;
    if (!backlightEnabled) backlightEnabled = toggleBacklight(true);
    inMenu = true;
    shouldRedrawMenu = true;
    if (game_running) {
        sound_pause();
        save_sram();
    }
    rvc_ui_prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    if (my_menu) {
        main_menu(game_running ? MAIN_MENU_RESUME : MAIN_MENU_LOAD_ROM);
    } else {
        multiplayer_wait_for_peer();
    }
    gspWaitForVBlank();
    if (guiop == 0) sound_resume();
    else if (guiop == AKILL) sound_refresh();
    else if (!(guiop & GUIEXIT)) sound_reset();
    inMenu = false;
    if (!oldBacklight) {
        if (my_menu || !is_multiplayer) shouldRedrawMenu = true;
        else backlightEnabled = toggleBacklight(false);
    }
}

void openPeerDisconnectMenu(void) {
    if (!backlightEnabled) {
        backlightEnabled = toggleBacklight(true);
        shouldRedrawMenu = true;
    }
    inMenu = true;
    shouldRedrawMenu = true;
    if (game_running) {
        sound_pause();
        save_sram();
    }
    rvc_ui_prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    multiplayer_error(0, &text_multi_disconnect, false);
    gspWaitForVBlank();
    if (guiop == 0) sound_resume();
    else if (guiop == AKILL) sound_refresh();
    else if (!(guiop & GUIEXIT)) sound_reset();
    inMenu = false;
}

static bool vsync_enabled = false;

void toggleVsync(bool enable) {
    // setup the thread
    endThread(frame_pacer_thread);
    u32 vtotal_top, vtotal_bottom;
    if (enable) {
        // 990 is closer to 50Hz but capture cards don't like when the two screens are out of sync
        vtotal_top = old_2ds || tVBOpt.ANAGLYPH ? 494 : 989;
        vtotal_bottom = 494;
        startPeriodicVsync(frame_pacer_thread);
    } else {
        vtotal_top = old_2ds || tVBOpt.ANAGLYPH ? 413 : 827;
        vtotal_bottom = 413;
        startPeriodic(frame_pacer_thread, 20000000, false);
    }
    // update VTotal only when necessary
    if (enable == vsync_enabled) return;
    vsync_enabled = enable;
    gspWaitForVBlank();
    GSPGPU_WriteHWRegs(0x400424, &vtotal_top, 4);
    GSPGPU_WriteHWRegs(0x400524, &vtotal_bottom, 4);
}

void toggleAnaglyph(bool enable) {
    bool old_vsync = vsync_enabled;
    // updating 3d mode resets VTotal, so turn off VSync in advance to fix the cache
    if (old_vsync) toggleVsync(false);
    tVBOpt.ANAGLYPH = enable;
    gfxSet3D(!enable);
    if (!old_vsync) return;
    // push 1 frame and wait for VBlank to reset VTotal
    C3D_FrameBegin(0);
    video_flush(true);
    C3D_FrameEnd(0);
    gspWaitForVBlank();
    // re-enable VSync if applicable
    toggleVsync(old_vsync);
}

void aptBacklight(APT_HookType hook, void* param) {
    if (hook == APTHOOK_ONSLEEP && !inMenu) {
        // don't save if we saved in the last 10 secs (unsure if this works)
        static u64 last_sleep_time = 0;
        if (osGetTime() - last_sleep_time >= 10000) {
            save_sram();
            last_sleep_time = osGetTime();
        }
    }
    if (tVBOpt.VSYNC) {
        switch (hook) {
            case APTHOOK_ONRESTORE:
            case APTHOOK_ONWAKEUP:
                toggleVsync(true);
                break;
            default:
                toggleVsync(false);
                break;
        }
    }
    if (!backlightEnabled) {
        switch (hook) {
            case APTHOOK_ONRESTORE:
            case APTHOOK_ONWAKEUP:
                toggleBacklight(false);
                break;
            default:
                toggleBacklight(true);
                break;
        }
    }
}

void showSoundError(void) {
    rvc_ui_prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    sound_error();
}

void showError(int code) {
    sound_pause();
    if (!backlightEnabled) toggleBacklight(true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    rvc_ui_prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C2D_TextBufClear(dynamic_textbuf);
    char buf[100];
    sprintf(buf, "DRC error #%d\nPC=0x%08lx\nDumping debug info...", code, vb_state->v810_state.PC);
    C2D_Text text;
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C3D_FrameBegin(0);
    C2D_TargetClear(screen, 0);
    C2D_SceneBegin(screen);
    C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 40, 0, 0.7, 0.7, TINT_COLOR);
    C2D_Flush();
    C3D_FrameEnd(0);

    drc_dumpDebugInfo(code);

    C3D_FrameBegin(0);
    C3D_FrameDrawOn(screen);
    C2D_DrawText(&text_debug_filenames, C2D_AlignCenter | C2D_WithColor, 320 / 2, 120, 0, 0.7, 0.7, TINT_COLOR);
    C2D_DrawText(&text_anykeyexit, C2D_AlignCenter | C2D_WithColor, 320 / 2, 180, 0, 0.7, 0.7, TINT_COLOR);
    C2D_Flush();
    C3D_FrameEnd(0);
}

void setCustomControls(void) {
    vbkey[__builtin_ctz(KEY_DUP)] = tVBOpt.CUSTOM_MAPPING_DUP;
    vbkey[__builtin_ctz(KEY_DDOWN)] = tVBOpt.CUSTOM_MAPPING_DDOWN;
    vbkey[__builtin_ctz(KEY_DLEFT)] = tVBOpt.CUSTOM_MAPPING_DLEFT;
    vbkey[__builtin_ctz(KEY_DRIGHT)] = tVBOpt.CUSTOM_MAPPING_DRIGHT;
    vbkey[__builtin_ctz(KEY_CPAD_UP)] = tVBOpt.CUSTOM_MAPPING_CPAD_UP;
    vbkey[__builtin_ctz(KEY_CPAD_DOWN)] = tVBOpt.CUSTOM_MAPPING_CPAD_DOWN;
    vbkey[__builtin_ctz(KEY_CPAD_LEFT)] = tVBOpt.CUSTOM_MAPPING_CPAD_LEFT;
    vbkey[__builtin_ctz(KEY_CPAD_RIGHT)] = tVBOpt.CUSTOM_MAPPING_CPAD_RIGHT;
    vbkey[__builtin_ctz(KEY_A)] = tVBOpt.CUSTOM_MAPPING_A;
    vbkey[__builtin_ctz(KEY_X)] = tVBOpt.CUSTOM_MAPPING_X;
    vbkey[__builtin_ctz(KEY_B)] = tVBOpt.CUSTOM_MAPPING_B;
    vbkey[__builtin_ctz(KEY_Y)] = tVBOpt.CUSTOM_MAPPING_Y;
    vbkey[__builtin_ctz(KEY_START)] = tVBOpt.CUSTOM_MAPPING_START;
    vbkey[__builtin_ctz(KEY_SELECT)] = tVBOpt.CUSTOM_MAPPING_SELECT;
    vbkey[__builtin_ctz(KEY_L)] = tVBOpt.CUSTOM_MAPPING_L;
    vbkey[__builtin_ctz(KEY_R)] = tVBOpt.CUSTOM_MAPPING_R;

    bool new_3ds = tVBOpt.CPP_ENABLED;
    if (!new_3ds) APT_CheckNew3DS(&new_3ds);
    if (new_3ds) {
        vbkey[__builtin_ctz(KEY_CSTICK_UP)] = tVBOpt.CUSTOM_MAPPING_CSTICK_UP;
        vbkey[__builtin_ctz(KEY_CSTICK_DOWN)] = tVBOpt.CUSTOM_MAPPING_CSTICK_DOWN;
        vbkey[__builtin_ctz(KEY_CSTICK_LEFT)] = tVBOpt.CUSTOM_MAPPING_CSTICK_LEFT;
        vbkey[__builtin_ctz(KEY_CSTICK_RIGHT)] = tVBOpt.CUSTOM_MAPPING_CSTICK_RIGHT;
        vbkey[__builtin_ctz(KEY_ZL)] = tVBOpt.CUSTOM_MAPPING_ZL;
        vbkey[__builtin_ctz(KEY_ZR)] = tVBOpt.CUSTOM_MAPPING_ZR;
    }
}

void setPresetControls(bool buttons) {
    shouldRedrawMenu = true;
    buttons_on_screen = buttons;
    if (buttons) {
        vbkey[__builtin_ctz(KEY_A)] = VB_RPAD_R;
        vbkey[__builtin_ctz(KEY_X)] = VB_RPAD_U;
        vbkey[__builtin_ctz(KEY_B)] = VB_RPAD_D;
        vbkey[__builtin_ctz(KEY_Y)] = VB_RPAD_L;
    } else {
        vbkey[__builtin_ctz(KEY_A)] = tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 4 ? VB_KEY_A : VB_KEY_B;
        vbkey[__builtin_ctz(KEY_Y)] = tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 5 ? VB_KEY_B : VB_KEY_A;
        vbkey[__builtin_ctz(KEY_B)] = tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 4 ? VB_KEY_B : VB_KEY_A;
        vbkey[__builtin_ctz(KEY_X)] = tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 5 ? VB_KEY_A : VB_KEY_B;
    }
    switch (tVBOpt.DPAD_MODE) {
        default: // VB LPAD
            vbkey[__builtin_ctz(KEY_DUP)] = VB_LPAD_U;
            vbkey[__builtin_ctz(KEY_DDOWN)] = VB_LPAD_D;
            vbkey[__builtin_ctz(KEY_DLEFT)] = VB_LPAD_L;
            vbkey[__builtin_ctz(KEY_DRIGHT)] = VB_LPAD_R;
            break;
        case 1: // VB RPAD
            vbkey[__builtin_ctz(KEY_DUP)] = VB_RPAD_U;
            vbkey[__builtin_ctz(KEY_DDOWN)] = VB_RPAD_D;
            vbkey[__builtin_ctz(KEY_DLEFT)] = VB_RPAD_L;
            vbkey[__builtin_ctz(KEY_DRIGHT)] = VB_RPAD_R;
            break;
        case 2: // Mirror ABXY buttons
            vbkey[__builtin_ctz(KEY_DUP)] = vbkey[__builtin_ctz(KEY_X)];
            vbkey[__builtin_ctz(KEY_DDOWN)] = vbkey[__builtin_ctz(KEY_B)];
            vbkey[__builtin_ctz(KEY_DLEFT)] = vbkey[__builtin_ctz(KEY_Y)];
            vbkey[__builtin_ctz(KEY_DRIGHT)] = vbkey[__builtin_ctz(KEY_A)];
            break;
    }
    bool new_3ds = tVBOpt.CPP_ENABLED;
    if (!new_3ds) APT_CheckNew3DS(&new_3ds);
    if (new_3ds) {
        vbkey[__builtin_ctz(KEY_L)] = tVBOpt.ZLZR_MODE <= 1 ? VB_KEY_L : tVBOpt.ZLZR_MODE == 2 ? VB_KEY_B : VB_KEY_A;
        vbkey[__builtin_ctz(KEY_R)] = tVBOpt.ZLZR_MODE <= 1 ? VB_KEY_R : tVBOpt.ZLZR_MODE == 2 ? VB_KEY_A : VB_KEY_B;
        vbkey[__builtin_ctz(KEY_ZL)] = tVBOpt.ZLZR_MODE == 0 ? VB_KEY_B : tVBOpt.ZLZR_MODE == 1 ? VB_KEY_A : VB_KEY_L;
        vbkey[__builtin_ctz(KEY_ZR)] = tVBOpt.ZLZR_MODE == 0 ? VB_KEY_A : tVBOpt.ZLZR_MODE == 1 ? VB_KEY_B : VB_KEY_R;
    }
}

bool guiShouldSwitch(void) {
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    return tVBOpt.TOUCH_SWITCH && touch_pos.px>=256 && touch_pos.px<304 && touch_pos.py>=12 && touch_pos.py<47;
}

void drawTouchControls(int inputs) {
    if(!inputs) inputs=guiGetInput(false);
    rvc_ui_image(RVC_SPRITES_NATIVE_CONTROLS_BACKGROUND,0,0,320,240);
    rvc_ui_image(RVC_SPRITES_NATIVE_II_NORMAL,tVBOpt.PAUSE_RIGHT/2-40,106,80,58);
    if(buttons_on_screen) {
        rvc_ui_image(RVC_SPRITES_NATIVE_A_NORMAL+!!(inputs & VB_KEY_A),tVBOpt.TOUCH_AX-26,tVBOpt.TOUCH_AY-26,52,58);
        rvc_ui_image(RVC_SPRITES_NATIVE_B_NORMAL+!!(inputs & VB_KEY_B),tVBOpt.TOUCH_BX-26,tVBOpt.TOUCH_BY-26,52,58);
        rvc_ui_text("A",tVBOpt.TOUCH_AX,tVBOpt.TOUCH_AY-41,true);
        rvc_ui_text("B",tVBOpt.TOUCH_BX,tVBOpt.TOUCH_BY-41,true);
    } else {
        int asset=RVC_SPRITES_NATIVE_DPAD_NORMAL;
        if(inputs & VB_RPAD_U) asset=RVC_SPRITES_NATIVE_DPAD_UP;
        else if(inputs & VB_RPAD_D) asset=RVC_SPRITES_NATIVE_DPAD_DOWN;
        else if(inputs & VB_RPAD_L) asset=RVC_SPRITES_NATIVE_DPAD_LEFT;
        else if(inputs & VB_RPAD_R) asset=RVC_SPRITES_NATIVE_DPAD_RIGHT;
        rvc_ui_image(asset,tVBOpt.TOUCH_PADX-56,tVBOpt.TOUCH_PADY-57,112,118);
    }
    if(tVBOpt.TOUCH_SWITCH) rvc_ui_image(RVC_SPRITES_NATIVE_SWAP_NORMAL,260,16,38,27);
    if(!old_2ds) rvc_ui_image(RVC_SPRITES_NATIVE_LIGHT_NORMAL,22,16,38,27);
    if(!is_multiplayer) rvc_ui_image(RVC_SPRITES_NATIVE_SPEED_NORMAL+!!tVBOpt.FASTFORWARD,141,16,38,27);
}

extern int input_state;

void guiUpdate(float total_time, float drc_time) {
    if (!backlightEnabled) return;

    static int last_inputs = 0;
    static bool last_fastforward = false;
    static bool last_saving = false;
    static bool last_replay = false;
    int new_inputs = guiGetInput(false);
    if (new_inputs != last_inputs) shouldRedrawMenu = true;
    last_inputs = new_inputs;
    if (last_fastforward != tVBOpt.FASTFORWARD) shouldRedrawMenu = true;
    last_fastforward = tVBOpt.FASTFORWARD;
    if (!!save_thread != last_saving) shouldRedrawMenu = true;
    last_saving = !!save_thread;
    if (replay_playing() != last_replay) shouldRedrawMenu = true;
    last_replay = replay_playing();

    shouldRedrawMenu=true; // Both displayed bottom buffers must receive the live UI.

    // unclear why this is necessary, but omitting it can lead to weird graphical glitches
    C3D_UpdateUniforms(GPU_GEOMETRY_SHADER);

    rvc_ui_prepare();
    C2D_SceneBegin(screen);

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    if (shouldRedrawMenu) {
        shouldRedrawMenu = false;
        C2D_TargetClear(screen, 0);
        drawTouchControls(0);
    }

    draw_status_bar(total_time, drc_time);

    if (save_thread) C2D_DrawText(&text_saving, C2D_AlignLeft, 0, 224, 0, 0.5, 0.5);

    if (tVBOpt.INPUTS) {
        int guiInputs = guiGetInput(false);
        u8 mods[16] = {3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3};
        if (guiInputs & VB_RPAD_U) mods[__builtin_ctz(VB_RPAD_U)] = 4;
        if (guiInputs & VB_RPAD_D) mods[__builtin_ctz(VB_RPAD_D)] = 4;
        if (guiInputs & VB_RPAD_L) mods[__builtin_ctz(VB_RPAD_L)] = 4;
        if (guiInputs & VB_RPAD_R) mods[__builtin_ctz(VB_RPAD_R)] = 4;
        if (guiInputs & VB_KEY_A) mods[__builtin_ctz(VB_KEY_A)] = 4;
        if (guiInputs & VB_KEY_B) mods[__builtin_ctz(VB_KEY_B)] = 4;
        for (int i = 0; i < 32; i++) {
            if ((input_state & (1 << i)) || (tVBOpt.CUSTOM_MOD[i] == 2 && (hidKeysHeld() & (1 << i)))) {
                int oldmod = mods[__builtin_ctz(vbkey[i])];
                if (tVBOpt.CUSTOM_MOD[i] != 0 || oldmod == 0 || oldmod == 3)
                    if (oldmod != 1) mods[__builtin_ctz(vbkey[i])] = tVBOpt.CUSTOM_MOD[i];
            }
        }
        // normal, toggle, turbo, up
        static u32 cols[] = {0xff808080, 0xff804000, 0xff000080, 0xff000000};
        // dpads
        C2D_DrawRectSolid(320/2-27, 8, 0, 54, 24, C2D_Color32(0, 0, 0, 255));
        C2D_DrawRectSolid(320/2-20-6, 16, 0, 12, 4, C2D_Color32(64, 64, 64, 255));
        C2D_DrawRectSolid(320/2+20-6, 16, 0, 12, 4, C2D_Color32(64, 64, 64, 255));
        C2D_DrawRectSolid(320/2-20-2, 12, 0, 4, 12, C2D_Color32(64, 64, 64, 255));
        C2D_DrawRectSolid(320/2+20-2, 12, 0, 4, 12, C2D_Color32(64, 64, 64, 255));
        for (int i = 0; i < 16; i++) {
            if ((1<<i) == VB_RPAD_U) C2D_DrawRectSolid(320/2+20-1, 13, 0, 2, 3, cols[mods[i]]);
            if ((1<<i) == VB_LPAD_U) C2D_DrawRectSolid(320/2-20-1, 13, 0, 2, 3, cols[mods[i]]);
            if ((1<<i) == VB_LPAD_D) C2D_DrawRectSolid(320/2-20-1, 20, 0, 2, 3, cols[mods[i]]);
            if ((1<<i) == VB_RPAD_D) C2D_DrawRectSolid(320/2+20-1, 20, 0, 2, 3, cols[mods[i]]);
            if ((1<<i) == VB_LPAD_L) C2D_DrawRectSolid(320/2-20-5, 17, 0, 3, 2, cols[mods[i]]);
            if ((1<<i) == VB_RPAD_L) C2D_DrawRectSolid(320/2+20-5, 17, 0, 3, 2, cols[mods[i]]);
            if ((1<<i) == VB_LPAD_R) C2D_DrawRectSolid(320/2-20+2, 17, 0, 3, 2, cols[mods[i]]);
            if ((1<<i) == VB_RPAD_R) C2D_DrawRectSolid(320/2+20+2, 17, 0, 3, 2, cols[mods[i]]);
        }
        // buttons
        C3D_ColorLogicOp(GPU_LOGICOP_COPY);
        C2D_DrawCircleSolid(320/2-12, 24, 0, 3, C2D_Color32(64, 64, 64, 255));
        C2D_DrawCircleSolid(320/2+12, 24, 0, 3, C2D_Color32(64, 64, 64, 255));
        C2D_DrawCircleSolid(320/2-6,  28, 0, 3, C2D_Color32(64, 64, 64, 255));
        C2D_DrawCircleSolid(320/2+6,  28, 0, 3, C2D_Color32(64, 64, 64, 255));
        C2D_DrawCircleSolid(320/2-8,  12, 0, 3, C2D_Color32(64, 64, 64, 255));
        C2D_DrawCircleSolid(320/2+8,  12, 0, 3, C2D_Color32(64, 64, 64, 255));
        for (int i = 0; i < 16; i++) {
            if ((1<<i) == VB_KEY_A)      C2D_DrawCircleSolid(320/2+12, 24, 0, 2, cols[mods[i]]);
            if ((1<<i) == VB_KEY_SELECT) C2D_DrawCircleSolid(320/2-12, 24, 0, 2, cols[mods[i]]);
            if ((1<<i) == VB_KEY_B)      C2D_DrawCircleSolid(320/2+6,  28, 0, 2, cols[mods[i]]);
            if ((1<<i) == VB_KEY_START)  C2D_DrawCircleSolid(320/2-6,  28, 0, 2, cols[mods[i]]);
            if ((1<<i) == VB_KEY_L)      C2D_DrawCircleSolid(320/2-8,  12, 0, 2, cols[mods[i]]);
            if ((1<<i) == VB_KEY_R)      C2D_DrawCircleSolid(320/2+8,  12, 0, 2, cols[mods[i]]);
        }
    }

    C2D_Flush();

    gpu_blend_default();
}

bool guiShouldPause(void) {
    touchPosition p;hidTouchRead(&p);
    if(p.py>=12 && p.py<47) return false;
    return p.px<tVBOpt.PAUSE_RIGHT && backlightEnabled;
}

int guiGetInput(bool ingame) {
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    if(!(hidKeysHeld() & KEY_TOUCH)) return 0;
    if (backlightEnabled) {
        if ((hidKeysHeld() & KEY_TOUCH) && guiShouldSwitch()) {
            if (ingame && (hidKeysDown() & KEY_TOUCH)) setPresetControls(!buttons_on_screen);
            return 0;
        }
        if (ingame) {
            if (!is_multiplayer) {
                if (touch_pos.px >= 137 && touch_pos.px<183 && touch_pos.py>=12 && touch_pos.py<47) {
                    if ((tVBOpt.FF_TOGGLE ? hidKeysDown() : hidKeysHeld()) & KEY_TOUCH) {
                        tVBOpt.FASTFORWARD = !tVBOpt.FASTFORWARD;
                    }
                    return 0;
                }
            }
            if (hidKeysDown() & KEY_TOUCH && (touch_pos.px>=18 && touch_pos.px<64 && touch_pos.py>=12 && touch_pos.py<47) && !old_2ds) {
                backlightEnabled = toggleBacklight(false);
                return 0;
            }
        }
        if (touch_pos.px < tVBOpt.PAUSE_RIGHT) {
            return 0;
        }
        if (buttons_on_screen) {
            int axdist = touch_pos.px - tVBOpt.TOUCH_AX;
            int aydist = touch_pos.py - tVBOpt.TOUCH_AY;
            int bxdist = touch_pos.px - tVBOpt.TOUCH_BX;
            int bydist = touch_pos.py - tVBOpt.TOUCH_BY;
            if (axdist*axdist + aydist*aydist < bxdist*bxdist + bydist*bydist)
                return VB_KEY_A;
            else
                return VB_KEY_B;
        } else {
            int xdist = touch_pos.px - tVBOpt.TOUCH_PADX;
            int ydist = touch_pos.py - tVBOpt.TOUCH_PADY;
            if (abs(xdist) >= abs(ydist)) {
                return xdist >= 0 ? VB_RPAD_R : VB_RPAD_L;
            } else {
                return ydist <= 0 ? VB_RPAD_U : VB_RPAD_D;
            }
        }
    } else if (hidKeysDown() & KEY_TOUCH) {
        backlightEnabled = toggleBacklight(true);
    }
    return 0;
}
