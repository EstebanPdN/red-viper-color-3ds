/* Approved semantic pixel artwork, recolored only when the interface tint changes. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <zlib.h>
#include "ui_native.h"
static C3D_Tex atlas;
static unsigned char *indices;
static int current_tint = -1;
static bool ready;
static unsigned morton(unsigned x, unsigned y) {
    return (x&1) | ((y&1)<<1) | ((x&2)<<1) | ((y&2)<<2) | ((x&4)<<2) | ((y&4)<<3);
}
bool rvc_ui_init(void) {
    FILE *f=fopen("romfs:/ui-atlas.bin","rb");
    if (!f) return false;
    fseek(f,0,SEEK_END); long len=ftell(f); rewind(f);
    unsigned char *packed=malloc(len); indices=malloc(RVC_UI_W*RVC_UI_H);
    if (!packed || !indices) { free(packed); free(indices); indices=NULL; fclose(f); return false; }
    size_t got=fread(packed,1,len,f); fclose(f);
    uLongf n=RVC_UI_W*RVC_UI_H;
    bool ok=got==(size_t)len && uncompress(indices,&n,packed,len)==Z_OK && n==RVC_UI_W*RVC_UI_H;
    free(packed);
    if (!ok || !C3D_TexInit(&atlas,RVC_UI_W,RVC_UI_H,GPU_RGBA8)) { free(indices); indices=NULL; return false; }
    C3D_TexSetFilter(&atlas,GPU_NEAREST,GPU_NEAREST);
    C3D_TexSetWrap(&atlas,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
    ready=true; return true;
}
void rvc_ui_theme(int tint) {
    if (!ready || current_tint==tint) return;
    current_tint=tint;
    if(tint==0x0000ff) tint=0x423beb;
    unsigned char p[256][4]={{0}};
    for(int i=1;i<256;i++) {
        for(int c=0;c<3;c++) p[i][c]=rvc_palette[i][c];
        p[i][3]=255;
        if(tint!=0x423beb && (i<16 || i>=33)) {
            float d=(rvc_palette[i][0]-rvc_palette[i][1])/176.f;
            float w=(rvc_palette[i][1]-59*d)/255.f;
            for(int c=0;c<3;c++) {
                float value=((tint>>(c*8))&255)*d+255*w;
                p[i][c]=(unsigned char)lroundf(fminf(255,fmaxf(0,value)));
            }
        }
    }
    unsigned char *out=atlas.data;
    for(unsigned y=0;y<RVC_UI_H;y++) for(unsigned x=0;x<RVC_UI_W;x++) {
        unsigned off=((y/8)*(RVC_UI_W/8)+(x/8))*64+morton(x&7,y&7);
        unsigned char *col=p[indices[y*RVC_UI_W+x]];
        out[off*4]=col[3]; out[off*4+1]=col[2]; out[off*4+2]=col[1]; out[off*4+3]=col[0];
    }
    C3D_TexFlush(&atlas);
}
static void region(int x,int y,int w,int h,float dx,float dy,float dw,float dh) {
    if(!ready) return;
    Tex3DS_SubTexture sub={.width=w,.height=h,.left=(float)x/RVC_UI_W,.right=(float)(x+w)/RVC_UI_W,.top=1.f-(float)y/RVC_UI_H,.bottom=1.f-(float)(y+h)/RVC_UI_H};
    C2D_Image im={.tex=&atlas,.subtex=&sub};
    C2D_DrawImageAt(im,dx,dy,0,NULL,dw/w,dh/h);
}
void rvc_ui_image(int asset,float x,float y,float w,float h) {
    if(asset<0 || asset>=RVC_ASSET_COUNT) return;
    RvcAsset a=rvc_assets[asset]; region(a.x,a.y,a.w,a.h,x,y,w?w:a.w,h?h:a.h);
}
void rvc_ui_patch(float x,float y,float w,float h,int state) {
    RvcAsset a=rvc_assets[RVC_SPRITES_BUTTON_PATCH_NORMAL+state];
    int b=7; int ss[]={0,b,a.w-b,a.w}; int sy[]={0,b,a.h-b,a.h}; float xx[]={x,x+b,x+w-b,x+w}; float yy[]={y,y+b,y+h-b,y+h};
    for(int j=0;j<3;j++) for(int i=0;i<3;i++) region(a.x+ss[i],a.y+sy[j],ss[i+1]-ss[i],sy[j+1]-sy[j],xx[i],yy[j],xx[i+1]-xx[i],yy[j+1]-yy[j]);
}
static RvcGlyph glyph_for(unsigned char ch) {
    unsigned code=ch<128?(unsigned)toupper(ch):'?';
    RvcGlyph glyph=rvc_glyphs[code];
    return glyph.w ? glyph : rvc_glyphs['?'];
}
static int line_width(const unsigned char *s) {
    int width=0;
    for(;*s && *s!='\n';s++) width+=glyph_for(*s).w+1;
    return width ? width-1 : 0;
}
void rvc_ui_text(const char *s,float x,float y,bool center) {
    RvcAsset a=rvc_assets[RVC_FONTS_UI_9PX];
    const unsigned char *line=(const unsigned char*)s;
    float top=floorf(y);
    while(*line) {
        float left=floorf(center?x-line_width(line)/2.f:x);
        const unsigned char *c=line;
        for(;*c && *c!='\n';c++) {
            RvcGlyph g=glyph_for(*c);
            region(a.x+g.x,a.y+g.y,g.w,9,left,top,g.w,9);
            left+=g.w+1;
        }
        if(!*c) break;
        line=c+1; top+=12;
    }
}
void rvc_ui_label(const char *text,float x,float y,float w,float h) {
    int lines=1;
    for(const char *p=text;*p;p++) if(*p=='\n') lines++;
    // The bitmap is nine pixels tall; twelve is the line advance, not its height.
    int ink_height=9+(lines-1)*12;
    rvc_ui_text(text,x+w/2.f,floorf(y+(h-ink_height)/2.f),true);
}
void rvc_ui_frame(const char *title) {
    rvc_ui_image(RVC_SPRITES_NATIVE_MENU_FRAME,0,0,320,240);
    rvc_ui_image(RVC_SPRITES_NATIVE_HEADER,21,12,278,22);
    rvc_ui_label(title,21,12,278,22);
}

void rvc_ui_prepare(void) {
    C3D_UpdateUniforms(GPU_GEOMETRY_SHADER);
    C2D_Prepare();
    // The emulator targets have no depth attachment. Reset inherited VIP state.
    C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_ALL);
    C3D_AlphaTest(false,GPU_ALWAYS,0);
    C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA);
}
