#include "../../../include/AssetInflate.h"
/* Generated atlas only. Bounded independent blocks, no heap or I/O. */
static bool face_alpha_read(uint32_t start,uint32_t count,uint8_t *out) {
    if(!out || start>FACE_ALPHA_BYTES || count>FACE_ALPHA_BYTES-start)return false;
    while(count) {
        unsigned block=start/4096u,skip=start%4096u;
        unsigned expected=FACE_ALPHA_BYTES-block*4096u;if(expected>4096u)expected=4096u;
        uint32_t pos=face_alpha_blocks[block],end=face_alpha_blocks[block+1];
        if(pos>end || end>sizeof(face_alpha_packed))return false;
        static uint8_t decoded[4096];static unsigned cached=UINT32_MAX;
        unsigned used=expected;
        if(cached!=block){
            cached=UINT32_MAX;
            if(tinf_uncompress(decoded,&used,face_alpha_packed+pos,end-pos)!=TINF_OK || used!=expected)return false;
            cached=block;
        }
        unsigned n=used-skip;if(n>count)n=count;
        memcpy(out,decoded+skip,n);out+=n;start+=n;count-=n;
    }
    return true;
}
static uint8_t face_alpha_glyph[FACE_ALPHA_GLYPH_MAX];
