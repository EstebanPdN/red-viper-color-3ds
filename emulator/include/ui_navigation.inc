static int rvc_row_icon(const char *s) {
    if(strstr(s,"ROM")) return RVC_SPRITES_ICONS_FOLDER_NORMAL;
    if(strstr(s,"Save") || strstr(s,"Profiles")) return RVC_SPRITES_ICONS_SAVE_NORMAL;
    if(strstr(s,"Color") || strstr(s,"Palette")) return RVC_SPRITES_ICONS_PALETTE_NORMAL;
    if(strstr(s,"Multiplayer")) return RVC_SPRITES_ICONS_NETWORK_NORMAL;
    if(strstr(s,"Sound")) return RVC_SPRITES_ICONS_SPEAKER_NORMAL;
    if(strstr(s,"Control")) return RVC_SPRITES_ICONS_CONTROLLER_NORMAL;
    if(strstr(s,"Resume")) return RVC_SPRITES_ICONS_PLAY_NORMAL;
    if(strstr(s,"About") || strstr(s,"Credit")) return RVC_SPRITES_ICONS_INFO_NORMAL;
    if(strstr(s,"Display")) return RVC_SPRITES_ICONS_EYE_NORMAL;
    if(strstr(s,"Restart") || strstr(s,"Restore")) return RVC_SPRITES_ICONS_RESET_NORMAL;
    if(strstr(s,"Exit")) return RVC_SPRITES_ICONS_EXIT_NORMAL;
    if(strstr(s,"Update") || strstr(s,"Transfer")) return RVC_SPRITES_ICONS_TRANSFER_NORMAL;
    return RVC_SPRITES_ICONS_SETTINGS_NORMAL;
}
/* Native navigation. Every row dispatches an existing emulator operation. */
static int rvc_list(const char *title, const char *const *labels, int count, int initial, const char *footer, const char *note) {
    Button rows[8] = {0};
    if(count>7) return -1;
    int step=count==7?22:count==6?26:count==5?29:34;
    int height=step-4;
    // Center the complete block inside the bezel, independent of row count.
    int block_height=count*step-4;
    int start=46+(154-block_height)/2;
    for(int i=0;i<count;i++) rows[i]=(Button){.str=(char*)labels[i],.x=31,.y=start+i*step,.w=258,.h=height,.icon_plus_one=strcmp(title,"CHOOSE ROM")?rvc_row_icon(labels[i])+1:0};
    rows[count]=(Button){.str=(char*)footer,.x=23,.y=211,.w=88,.h=26,.icon_plus_one=RVC_SPRITES_ICONS_BACK_NORMAL+1};
    selectedButton=&rows[initial>=0 && initial<=count?initial:0];
    buttonLock=true;
    while(aptMainLoop()) {
        rvc_ui_theme(tVBOpt.TINT);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        video_flush(true);
        if(!game_running) draw_logo();
        C2D_TargetClear(screen,0); C2D_SceneBegin(screen); rvc_ui_prepare();
        hidScanInput(); if(!hidKeysHeld()) buttonLock=false;
        rvc_ui_frame(title);
        if(note) rvc_ui_text(note,210,220,true);
        else rvc_ui_text("A SELECT",241,220,false);
        int selected=handle_buttons(rows,count+1);
        C2D_Flush(); C3D_FrameEnd(0);
        if(selected>=0) { selectedButton=NULL; return selected; }
    }
    guiop=GUIEXIT; selectedButton=NULL; return -1;
}
static int rvc_rom_list(const char *const *labels,int files,const char *note) {
    Button rows[8]={0};
    for(int i=0;i<files;i++) rows[i]=(Button){.str=(char*)labels[i],.x=31,.y=73+i*26,.w=258,.h=22};
    rows[files]=(Button){.str="Parent Folder",.x=31,.y=41,.w=258,.h=22,.secondary=true,.icon_plus_one=RVC_SPRITES_ICONS_UP_NORMAL+1};
    rows[files+1]=(Button){.str="Previous Page",.x=31,.y=182,.w=122,.h=24,.secondary=true};
    rows[files+2]=(Button){.str="Next Page",.x=167,.y=182,.w=122,.h=24,.secondary=true};
    rows[files+3]=(Button){.str="Back",.x=23,.y=211,.w=88,.h=26,.icon_plus_one=RVC_SPRITES_ICONS_BACK_NORMAL+1};
    selectedButton=&rows[files?0:files];buttonLock=true;
    while(aptMainLoop()) {
        rvc_ui_theme(tVBOpt.TINT);C3D_FrameBegin(C3D_FRAME_SYNCDRAW);video_flush(true);
        if(!game_running) draw_logo();
        C2D_TargetClear(screen,0);C2D_SceneBegin(screen);rvc_ui_prepare();hidScanInput();
        if(!hidKeysHeld()) buttonLock=false;
        rvc_ui_frame("CHOOSE ROM");rvc_ui_text(note,210,220,true);
        if(!files) rvc_ui_text("NO ROMS IN THIS FOLDER",160,104,true);
        int result=handle_buttons(rows,files+4);C2D_Flush();C3D_FrameEnd(0);
        if(result>=0) {selectedButton=NULL;return result;}
    }
    guiop=GUIEXIT;selectedButton=NULL;return -1;
}
static void rvc_message(const char *title, const char *line1, const char *line2) {
    Button back={.str="Back",.x=23,.y=211,.w=88,.h=26};
    selectedButton=&back; buttonLock=true;
    while(aptMainLoop()) {
        rvc_ui_theme(tVBOpt.TINT);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW); video_flush(true); if(!game_running) draw_logo();
        C2D_TargetClear(screen,0); C2D_SceneBegin(screen); rvc_ui_prepare(); hidScanInput(); if(!hidKeysHeld()) buttonLock=false;
        rvc_ui_frame(title); rvc_ui_text(line1,160,89,true); rvc_ui_text(line2,160,111,true);
        int b=handle_buttons(&back,1); C2D_Flush(); C3D_FrameEnd(0);
        if(b>=0) { selectedButton=NULL; return; }
    }
    guiop=GUIEXIT; selectedButton=NULL;
}
static void rvc_about(void) {
    const char *labels[]={"Project","Color Mode","Original Emulator","Acknowledgments"};
    int focus=0;
    for(;;) {
        int b=rvc_list("ABOUT",labels,4,focus,"Back",NULL);
        if(b<0 || b==4) return;
        focus=b;
        Button back={.str="Back",.x=23,.y=211,.w=88,.h=26};
        selectedButton=&back; buttonLock=true;
        while(aptMainLoop()) {
            rvc_ui_theme(tVBOpt.TINT); C3D_FrameBegin(C3D_FRAME_SYNCDRAW); video_flush(true); if(!game_running) draw_logo();
            C2D_TargetClear(screen,0); C2D_SceneBegin(screen); rvc_ui_prepare(); hidScanInput(); if(!hidKeysHeld()) buttonLock=false;
            rvc_ui_image(RVC_MENUS_CREDITS_PROJECT+b,0,0,320,240);
            int ret=handle_buttons(&back,1); C2D_Flush(); C3D_FrameEnd(0);
            if(ret>=0) break;
        }
        selectedButton=NULL;
        if(!aptMainLoop()) { guiop=GUIEXIT; return; }
    }
}
static void rvc_pick_color(void) {
    static const char *labels[]={"Red","Blue","Gray","White","Amber","Green","Custom RGB"};
    static const int colors[]={0x423beb,0xe88b40,0xa4a4a4,0xeeeeee,0x46c7f1,0x73cd61};
    int focus=0;
    for(;;) {
        int b=rvc_list("PICK COLOR",labels,7,focus,"Back",NULL);
        if(b<0 || b==7) return;
        focus=b;
        if(b<6) { tVBOpt.TINT=colors[b]; tVBOpt.MODIFIED=true; }
        else if(swkbd_colour(&tVBOpt.TINT)) tVBOpt.MODIFIED=true;
    }
}
static void rvc_profiles(void) {
    const char *labels[]={"Save for current game","Save globally","Restore global settings","Discard changes"};
    int focus=0;
    for(;;) {
        int b=rvc_list("PROFILES",labels,4,focus,"Back",tVBOpt.GAME_SETTINGS?"CURRENT SCOPE: GAME":"CURRENT SCOPE: GLOBAL");
        if(b<0 || b==4) return;
        focus=b;
        if(b==0) { if(game_running) saveGameOptions(); else rvc_message("NO GAME LOADED","LOAD A ROM FIRST","THEN SAVE A GAME PROFILE"); }
        if(b==1) { if(tVBOpt.GAME_SETTINGS) deleteGameOptions(); saveFileOptions(); }
        if(b==2) { if(tVBOpt.GAME_SETTINGS) deleteGameOptions(); loadFileOptions(); }
        if(b==3) { loadFileOptions(); if(game_running) loadGameOptions(); }
    }
}
static void rvc_sound(void) {
    int focus=0;
    for(;;) {
        const char *labels[]={tVBOpt.SOUND?"Sound: On":"Sound: Off",tVBOpt.FF_TOGGLE?"Fast Forward: Toggle":"Fast Forward: Hold"};
        int b=rvc_list("SOUND AND SPEED",labels,2,focus,"Back",NULL);
        if(b<0 || b==2) return;
        focus=b;
        if(b==0) { tVBOpt.SOUND=!tVBOpt.SOUND; tVBOpt.MODIFIED=true; }
        if(b==1) { tVBOpt.FF_TOGGLE=!tVBOpt.FF_TOGGLE; tVBOpt.MODIFIED=true; }
    }
}
static void rvc_image_color(void) {
    int focus=0;
    for(;;) {
        const char *labels[]={tVBOpt.FULL_COLOR?"Color Mode: Full Color":"Color Mode: Single Color","Pick Color","Display / 3D"};
        int b=rvc_list("IMAGE AND COLOR",labels,3,focus,"Back",rvc_pack_status());
        if(b<0 || b==3) return;
        focus=b;
        if(b==0) { tVBOpt.FULL_COLOR = !tVBOpt.FULL_COLOR; rvc_reset(); tVBOpt.MODIFIED=true; }
        if(b==1) rvc_pick_color();
        if(b==2) video_settings(0);
        if(guiop & GUIEXIT) return;
    }
}
static void rvc_advanced(void) {
    int focus=0;
    const char *labels[]={"Performance","Legacy Palettes","VBLink Transfer","Credits"};
    for(;;) {
        int b=rvc_list("ADVANCED",labels,4,focus,"Back",NULL);
        if(b<0 || b==4) return;
        focus=b;
        if(b==0) dev_options(0);
        if(b==1) multicolour_picker(0);
        if(b==2) { if(!tVBOpt.FORWARDER) vblink(); else rvc_message("VBLINK TRANSFER","UNAVAILABLE IN FORWARDERS","OPEN THE STANDALONE APP"); }
        if(b==3) rvc_about();
        if(guiop) return;
    }
}
static void rvc_settings(void) {
    const char *labels[]={"Image and Color","Controls","Sound and Speed","Profiles","Advanced"};
    int focus=0;
    for(;;) {
        int b=rvc_list("SETTINGS",labels,5,focus,"Back",tVBOpt.MODIFIED?"UNSAVED SETTINGS":NULL);
        if(b<0) return;
        if(b==5) {
            if(!tVBOpt.MODIFIED) return;
            const char *choices[]={game_running?"Save for current game":"Save globally","Discard changes","Keep editing"};
            int c=rvc_list("UNSAVED SETTINGS",choices,3,2,"Back",NULL);
            if(c==0) { if(game_running) saveGameOptions(); else saveFileOptions(); return; }
            if(c==1) { loadFileOptions(); if(game_running) loadGameOptions(); return; }
            if(c<0) return;
            continue;
        }
        focus=b;
        if(b==0) rvc_image_color();
        if(b==1) controls(0);
        if(b==2) rvc_sound();
        if(b==3) rvc_profiles();
        if(b==4) rvc_advanced();
        if(guiop) return;
    }
}
