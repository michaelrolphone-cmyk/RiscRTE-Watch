/* Shared exact card composition; source pixels and ordered underlay are unchanged. */
static void picker_composite_reference(canvas*c,const uint16_t*scratch,int size,int x0,int y0,int radius,int border,unsigned opacity,uint16_t border_color,bool legacy) {
        for(int y=0;y<size;y++)for(int x=0;x<size;x++)if(x+x0>=0&&x+x0<(int)c->width&&y+y0>=0&&y+y0<(int)c->height){
            int edge=x<radius?radius-x:x>=size-radius?x-(size-radius-1):0,ey=y<radius?radius-y:y>=size-radius?y-(size-radius-1):0;
            if(edge&&ey&&edge*edge+ey*ey>radius*radius)continue;
            uint16_t col=scratch[legacy?(y*240/size)*240+x*240/size:y*size+x];
            unsigned fade=c->width*46u/240u;if(!fade)fade=1;
            int xx=x+x0;unsigned mask=(unsigned)xx<fade?(unsigned)xx*255/fade:
                xx>(int)(c->width-fade)?(c->width-1u-(unsigned)xx)*255/(fade>1?fade-1:1):255;
            blend(at(c,x+x0,y+y0),col,opacity*mask/255);
            if(x<border||x>=size-border||y<border||y>=size-border||(edge&&ey&&edge*edge+ey*ey>=(radius-border)*(radius-border)))blend(at(c,x+x0,y+y0),border_color,opacity*mask/255);
        }
}
#ifdef WATCH_PICKER_COMPOSITOR_PLAN
/* Row address is only a derived hint. Delegate every original checkpoint
 * access to at(), then refresh after the callback before later accesses. */
static inline uint8_t *picker_row_at(canvas*c,unsigned x,unsigned y,uint8_t**row) {
#ifdef WATCH_CONTEXTS_CLIENT
    if(c->failed)return c->sink;
    if(!((c->operations+1u)&255u)) {
        uint8_t*p=at(c,x,y);
        if(!c->failed)*row=c->pixels+y*c->stride;
        return p;
    }
    ++c->operations;
#else
    (void)c;(void)y;
#endif
    return *row+2u*x;
}
/* One owned optional column workspace is reused serially for each card. No
 * rendered underlay, sampled coverage or frame lease is cached here. */
static void picker_composite_planned(canvas*c,const uint16_t*scratch,int size,int x0,int y0,int radius,int border,unsigned opacity,uint16_t border_color,bool legacy,uint32_t*columns,unsigned capacity) {
    if(!columns||size<=0||(unsigned)size>capacity||radius<0||radius>size/2||border<0||opacity>255u){picker_composite_reference(c,scratch,size,x0,y0,radius,border,opacity,border_color,legacy);return;}
#ifdef WATCH_CONTEXTS_CLIENT
    if(c->failed){picker_composite_reference(c,scratch,size,x0,y0,radius,border,opacity,border_color,legacy);return;}
#endif
    int first=0,last=size-1,top=0,bottom=size-1;
    if(x0<0)first=-x0;
    if(y0<0)top=-y0;
    if(last>(int)c->width-1-x0)last=(int)c->width-1-x0;
    if(bottom>(int)c->height-1-y0)bottom=(int)c->height-1-y0;
    if(first>last||top>bottom)return;
    unsigned fade=c->width*46u/240u;if(!fade)fade=1;
    for(int x=first;x<=last;x++){
        int xx=x+x0;unsigned mask=(unsigned)xx<fade?(unsigned)xx*255/fade:
            xx>(int)(c->width-fade)?(c->width-1u-(unsigned)xx)*255/(fade>1?fade-1:1):255;
        unsigned source_x=legacy?(unsigned)(x*240/size):(unsigned)x;
        columns[x]=(source_x<<8)|(opacity*mask/255);
    }
    const int outer_squared=radius*radius,inner_squared=(radius-border)*(radius-border);
    for(int y=top;y<=bottom;y++){
        int ey=y<radius?radius-y:y>=size-radius?y-(size-radius-1):0;
        int clipped=ey?radius-(int)watch_circle_root32((uint32_t)(outer_squared-ey*ey)):0;
        int row_first=first>clipped?first:clipped;
        int row_last=last<size-1-clipped?last:size-1-clipped;
        if(row_first>row_last)continue;
        int border_end=border-1;
        if(ey){int remainder=inner_squared-ey*ey;
            int minimum_edge=remainder<=1?1:(int)watch_circle_root32((uint32_t)(remainder-1))+1;
            if(radius-minimum_edge>border_end)border_end=radius-minimum_edge;
        }
        bool whole_border=y<border||y>=size-border;
        uint8_t*destination=NULL;
#ifdef WATCH_CONTEXTS_CLIENT
        if(!c->failed)
#endif
        destination=c->pixels+(unsigned)(y+y0)*c->stride;
        const uint16_t*row=scratch+(legacy?(y*240/size)*240:y*size);
        for(int x=row_first;x<=row_last;x++){
            uint32_t planned=columns[x];unsigned alpha=planned&255u;
            blend(picker_row_at(c,x+x0,y+y0,&destination),row[planned>>8],alpha);
            if(whole_border||x<=border_end||x>=size-1-border_end)blend(picker_row_at(c,x+x0,y+y0,&destination),border_color,alpha);
        }
    }
}
#endif
