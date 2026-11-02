/* Rasterize native primitives and individual licensed masks into one native
 * scanline. The final image is never sampled through a smaller framebuffer. */
static unsigned native_alpha(const uint8_t *mask,int width,int height,int x,int y,bool packed) {
    if(x<0 || y<0 || x>=width || y>=height)return 0;
    unsigned index=(unsigned)(y*width+x);
    return packed?((mask[index/2]>>((index&1u)*4u))&15u)*17u:mask[index];
}
static void native_mask(pqa_row *r,const uint8_t *mask,int sw,int sh,
                        int x,int y,int w,int h,uint16_t color,unsigned alpha,bool packed) {
    if(w<1 || h<1 || r->y<y || r->y>=y+h)return;
    int fy=((2*(r->y-y)+1)*sh*128)/h-128;
    int iy=fy>=0?fy/256:-((-fy+255)/256),ay=fy-iy*256;
    int first=x<0?-x:0,last=x+w>r->width?r->width-x:w;
    for(int dx=first;dx<last;++dx) {
        int fx=((2*dx+1)*sw*128)/w-128;
        int ix=fx>=0?fx/256:-((-fx+255)/256),ax=fx-ix*256;
        unsigned top=native_alpha(mask,sw,sh,ix,iy,packed)*(256-ax)+native_alpha(mask,sw,sh,ix+1,iy,packed)*ax;
        unsigned bottom=native_alpha(mask,sw,sh,ix,iy+1,packed)*(256-ax)+native_alpha(mask,sw,sh,ix+1,iy+1,packed)*ax;
        unsigned a=(top*(256-ay)+bottom*ay+32768u)/65536u;
        if(a)pixel(r,x+dx,color,(a*alpha+127u)/255u);
    }
}
static int native_metric(int value,int unit,int divisor) {
    return (value*unit+(value<0?-divisor/2:divisor/2))/divisor;
}
static int native_text_width(const pqa_glyph *g,size_t n,const char *s,unsigned limit,int unit) {
    int advance=0;
    for(unsigned i=0;i<limit && s[i];++i) {const pqa_glyph *c=glyph(g,n,s[i]);if(c)advance+=c->advance_q8;}
    return (advance*unit+61440)/122880;
}
static void native_text(pqa_row *r,const pqa_glyph *g,size_t n,const char *s,unsigned limit,
                        int x,int baseline,int unit,uint16_t color,unsigned alpha) {
    int advance=0;
    for(unsigned i=0;i<limit && s[i];++i) {
        const pqa_glyph *c=glyph(g,n,s[i]);if(!c)continue;
        int left=x+(advance*unit+61440)/122880+native_metric(c->left,unit,480);
        int top=baseline+native_metric(c->top,unit,480);
        int width=native_metric(c->width,unit,480),height=native_metric(c->height,unit,480);
        native_mask(r,pqa_native_alpha+c->offset,c->width,c->height,left,top,width,height,color,alpha,true);
        advance+=c->advance_q8;
    }
}
#define NATIVE_GLYPHS(set) pqa_native_##set##_glyphs,sizeof(pqa_native_##set##_glyphs)/sizeof(pqa_native_##set##_glyphs[0])
static void native_center(pqa_row *r,const pqa_glyph *g,size_t n,const char *s,unsigned limit,
                          int cx,int baseline,int unit,uint16_t color,unsigned alpha) {
    native_text(r,g,n,s,limit,cx-native_text_width(g,n,s,limit,unit)/2,baseline,unit,color,alpha);
}
static void native_icon(pqa_row *r,const pqa_layout *l,const pqa_icon *ic,int cx,int y,uint16_t color,unsigned alpha) {
    int w=pqa_layout_size(l,ic->width),h=pqa_layout_size(l,ic->height);
    native_mask(r,pqa_alpha+ic->offset,ic->width,ic->height,cx-w/2,y,w,h,color,alpha,false);
}
static void native_slider(pqa_row *r,const pqa_layout *l,int index,const pqa_icon *ic,
                          unsigned value,bool valid,bool muted,bool error) {
    unsigned a=muted?115u:256u;int y=l->slider_y[index];
    native_icon(r,l,ic,pqa_layout_x(l,28),y-pqa_layout_size(l,1),error?RGB(255,135,65):CYAN,a);
    int span=l->slider_right-l->slider_left+3*pqa_layout_size(l,1);
    int gap=pqa_layout_size(l,3);
    for(int i=0;i<10;++i) {
        int x=l->slider_left+i*span/10,next=l->slider_left+(i+1)*span/10;
        rect(r,x,y,next-x>gap?next-x-gap:1,pqa_layout_size(l,14),pqa_layout_size(l,3),CYAN,
             valid&&(unsigned)i*10<value?a:a*41u/256u);
    }
    char label[5];percentage(label,valid,value);
    native_text(r,NATIVE_GLYPHS(percent),label,4,pqa_layout_x(l,220)-native_text_width(NATIVE_GLYPHS(percent),label,4,l->unit),
                y+pqa_layout_scale(l,11),l->unit,error?RGB(255,135,65):RGB(207,233,238),a);
}
static void native_panel_row(pqa_row *r,const pqa_state *s,const pqa_layout *l,const char *time,bool bv,unsigned bp) {
    native_text(r,NATIVE_GLYPHS(time),time?time:"--:--",8,pqa_layout_x(l,20),pqa_layout_y(l,s->contexts_controls?19:31),l->unit,RGB(255,255,255),256);
    char battery[5];percentage(battery,bv,bp);
    native_text(r,NATIVE_GLYPHS(percent),battery,4,pqa_layout_x(l,146)-native_text_width(NATIVE_GLYPHS(percent),battery,4,l->unit),
                pqa_layout_y(l,s->contexts_controls?18:30),l->unit,bv?GREEN:MUTED,256);
    for(int i=0;i<10;++i)rect(r,pqa_layout_x(l,152+i*7),pqa_layout_y(l,s->contexts_controls?12:24),
        pqa_layout_size(l,5),pqa_layout_size(l,4),pqa_layout_size(l,1),GREEN,bv&&(unsigned)i*10<bp?256:41);
    native_slider(r,l,0,&pqa_icon_sun,s->brightness,s->brightness_valid,false,(s->error_flags&PQA_ERROR_BRIGHTNESS)!=0);
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    const char *volume_caption=s->audio_available?"NOTIFICATIONS":"VOLUME PREF ONLY";
#else
    const char *volume_caption="NOTIFICATIONS";
#endif
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    native_text(r,NATIVE_GLYPHS(caption),volume_caption,16,l->slider_left,l->slider_y[1]-pqa_layout_scale(l,5),l->unit,RGB(207,233,238),256);
#else
    native_text(r,NATIVE_GLYPHS(caption),volume_caption,13,l->slider_left,l->slider_y[1]-pqa_layout_scale(l,5),l->unit,MUTED,210);
#endif
    native_slider(r,l,1,s->volume_valid&&!s->volume?&pqa_icon_muted:&pqa_icon_volume,s->volume,s->volume_valid,s->volume_valid&&!s->volume,(s->error_flags&PQA_ERROR_VOLUME)!=0);
    rect(r,pqa_layout_x(l,20),l->slider_y[1]+pqa_layout_scale(l,23),pqa_layout_x(l,220)-pqa_layout_x(l,20),pqa_layout_size(l,1),0,LINE,256);
    static const char *const labels[]={"SILENT","DND","AIRPLANE","WI-FI SETUP","BLUETOOTH","TORCH"};
    const pqa_icon *const icons[]={&pqa_icon_silent,&pqa_icon_dnd,&pqa_icon_airplane,&pqa_icon_wifi,&pqa_icon_bluetooth,&pqa_icon_torch};
    for(int i=0;i<6;++i) {
        pqa_rect tile=l->tiles[i];
        bool disabled=(i==1&&!s->dnd_valid)||((i==2||i==4||(i==3&&s->radio_controls))&&!s->radios_valid)||(i==0&&!s->volume_valid);
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
        if(i==2)disabled=!s->radios_valid;
        if(i==3 && s->radio_controls)disabled=!s->wifi_valid;
        if(i==4)disabled=!s->bluetooth_valid;
#endif
        bool on=(i==1&&s->dnd_valid&&s->dnd_enabled)||(i==0&&s->volume_valid&&!s->volume)||(i==5&&s->torch)||(s->radios_valid&&((i==2&&s->airplane)||(i==3&&s->wifi_enabled)||(i==4&&s->bluetooth_enabled)));
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
        if(i==3)on=s->wifi_valid&&s->wifi_enabled;
        if(i==4)on=s->bluetooth_valid&&s->bluetooth_enabled;
#endif
        int border=pqa_layout_size(l,1),radius=pqa_layout_size(l,12);
        rect(r,tile.x,tile.y,tile.w,tile.h,radius,on?CYAN:LINE,256);
        if(!on)rect(r,tile.x+border,tile.y+border,tile.w-2*border,tile.h-2*border,radius-border,TILE,256);
        #ifdef PORTABLE_QUICK_CAPABILITY_SAFE
        uint16_t color=on?RGB(0,20,24):disabled?RGB(150,162,168):RGB(207,233,238);
#else
        uint16_t color=on?RGB(0,20,24):disabled?RGB(66,82,88):MUTED;
#endif
        if((i==3&&(s->error_flags&PQA_ERROR_WIFI))||(i==1&&(s->error_flags&PQA_ERROR_DND)))color=RGB(255,135,65);
        native_icon(r,l,icons[i],tile.x+tile.w/2,tile.y+pqa_layout_scale(l,6+(18-icons[i]->height)/2),color,256);
        const char *label=i==3&&s->radio_controls?"WI-FI":labels[i];
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
        if(disabled&&i==2)label="NOT READY";
        if(disabled&&i==3)label=s->wifi_available?"CHECK WI-FI":"NO WI-FI";
        if(disabled&&i==4)label=s->bluetooth_available?"CHECK BT":"NO BT";
#endif
        native_center(r,NATIVE_GLYPHS(label),label,12,tile.x+tile.w/2,tile.y+pqa_layout_scale(l,36),l->unit,color,256);
        if(disabled)rect(r,tile.x+tile.w-pqa_layout_size(l,13),tile.y+pqa_layout_scale(l,6),pqa_layout_size(l,5),pqa_layout_size(l,1),0,RGB(66,82,88),256);
    }
    if(s->contexts_controls) {
        bool available=s->contexts_valid;
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
        available=available&&s->contexts_available;
#endif
        pqa_rect tile=l->contexts;bool on=available&&s->contexts_enabled;
        uint16_t color=available?CYAN:MUTED;
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
        if(!available)color=RGB(150,162,168);
#endif
        int border=pqa_layout_size(l,1),radius=pqa_layout_size(l,12);
        rect(r,tile.x,tile.y,tile.w,tile.h,radius,color,256);
        rect(r,tile.x+border,tile.y+border,tile.w-2*border,tile.h-2*border,radius-border,TILE,256);
        native_text(r,NATIVE_GLYPHS(label),"CONTEXT",7,pqa_layout_x(l,31),tile.y+pqa_layout_scale(l,16),l->unit,color,256);
        native_text(r,NATIVE_GLYPHS(label),"DETECTION",9,pqa_layout_x(l,31),tile.y+pqa_layout_scale(l,30),l->unit,color,256);
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
        if(!s->contexts_available) {
            native_center(r,NATIVE_GLYPHS(label),"UNAVAILABLE",12,pqa_layout_x(l,175),tile.y+pqa_layout_scale(l,23),l->unit,RGB(207,233,238),256);
        } else {
#endif
        rect(r,pqa_layout_x(l,143),tile.y+pqa_layout_scale(l,9),pqa_layout_x(l,208)-pqa_layout_x(l,143),pqa_layout_size(l,22),pqa_layout_size(l,10),LINE,256);
        rect(r,pqa_layout_x(l,on?176:145),tile.y+pqa_layout_scale(l,11),pqa_layout_x(l,175)-pqa_layout_x(l,145),pqa_layout_size(l,18),pqa_layout_size(l,8),on?CYAN:LINE,256);
        native_center(r,NATIVE_GLYPHS(label),available?"OFF":"?",3,pqa_layout_x(l,160),tile.y+pqa_layout_scale(l,23),l->unit,on?MUTED:RGB(255,255,255),256);
        native_center(r,NATIVE_GLYPHS(label),"ON",2,pqa_layout_x(l,192),tile.y+pqa_layout_scale(l,23),l->unit,on?RGB(0,20,24):MUTED,256);
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
        }
#endif
    }
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    const char *save_label=s->save_pending?"SAVE NOT CONFIRMED":
        (s->gesture==PQA_BRIGHTNESS_DRAG||s->gesture==PQA_VOLUME_DRAG)?"PREVIEW - NOT SAVED":
        (s->error_flags&PQA_ERROR_SAVE)?"SAVE UNCONFIRMED":NULL;
    int handle_y=l->origin_y+l->content_height-pqa_layout_size(l,save_label?4:s->contexts_controls?13:24);
    rect(r,l->origin_x+l->content_width/2-pqa_layout_size(l,16),handle_y,pqa_layout_size(l,32),pqa_layout_size(l,4),pqa_layout_size(l,2),CYAN,128);
    if(save_label)native_center(r,NATIVE_GLYPHS(caption),save_label,strlen(save_label),l->origin_x+l->content_width/2,l->origin_y+l->content_height-pqa_layout_size(l,10),l->unit,RGB(255,135,65),256);
#else
    int handle_y=l->origin_y+l->content_height-pqa_layout_size(l,s->contexts_controls?13:24);
#ifdef PORTABLE_QUICK_SAFE_CONTENT
    if(s->error_flags&PQA_ERROR_SAVE)handle_y=l->origin_y+l->content_height-pqa_layout_size(l,4);
#endif
    rect(r,l->origin_x+l->content_width/2-pqa_layout_size(l,16),handle_y,pqa_layout_size(l,32),pqa_layout_size(l,4),pqa_layout_size(l,2),CYAN,128);
    if(s->error_flags&PQA_ERROR_SAVE)native_center(r,NATIVE_GLYPHS(caption),"SAVE UNCONFIRMED",16,l->origin_x+l->content_width/2,l->origin_y+l->content_height-pqa_layout_size(l,9),l->unit,RGB(255,135,65),256);
#endif
}
static bool native_render_rows(risc_display_surface_v1 *f,const pqa_state *s,const char *time,bool bv,uint8_t bp,unsigned first,unsigned count) {
    if(!s->native_width || s->native_width>2048 || !s->logical_height || s->logical_height>2048 ||
       f->width!=s->native_width || f->height!=s->logical_height ||
       first>f->height || count>f->height-first || f->stride_bytes<(uint32_t)f->width*2u)return false;
    uint64_t span=(uint64_t)(f->height-1)*f->stride_bytes+(uint32_t)f->width*2u;
    if(span>f->size_bytes)return false;
    if(!pqa_visible(s))return true;
    pqa_layout l=pqa_layout_for(s);bv=bv&&bp<=100;
    int open=pqa_open_q8(s),position=s->position_q8<0?0:s->position_q8>open?open:s->position_q8;
    int offset=(position-open)/256,fade=pqa_layout_size(&l,22),dim=pqa_layout_size(&l,190);
    unsigned face_alpha=position>=dim*256?0:256-(unsigned)position/(unsigned)dim;
    /* Validated 1..2048: 820 bytes on 410px hardware, never a full image. */
    uint16_t pixels[f->width];pqa_row row={pixels,0,l.width};
    for(unsigned py=first;py<first+count;++py) {
        uint8_t *dst=(uint8_t*)f->pixels+(size_t)py*f->stride_bytes;
        if(s->torch) {
            row.y=(int)py;for(int x=0;x<l.width;++x)pixels[x]=RGB(255,243,214);
            native_center(&row,NATIVE_GLYPHS(torch),"TAP TO TURN OFF",15,l.origin_x+l.content_width/2,l.origin_y+l.content_height-pqa_layout_size(&l,30),l.unit,RGB(106,90,48),256);
            for(int x=0;x<l.width;++x) {
                unsigned a=torch_edge(x*240/l.unit,l.width*240/l.unit)*torch_edge((int)py*240/l.unit,l.height*240/l.unit)/256u;
                write_pixel(dst+2*x,mix(0,pixels[x],a));
            }
            continue;
        }
        int local=(int)py-offset;bool in_panel=local>=0&&local<l.height;
        unsigned alpha=in_panel?(local<l.height-fade?256u:(unsigned)(l.height-local)*256u/(unsigned)fade):0;
        if(in_panel) {memset(pixels,0,(size_t)l.width*sizeof(*pixels));row.y=local;native_panel_row(&row,s,&l,time,bv,bp);}
        for(int x=0;x<l.width;++x) {
            uint16_t behind=mix(0,read_pixel(dst+2*x),face_alpha);
            write_pixel(dst+2*x,in_panel?mix(behind,pixels[x],alpha):behind);
        }
    }
    return true;
}
