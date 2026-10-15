/* Independent 256-byte RLE/LZ blocks, no heap, history, or service I/O.
 * 0xxxxxxx: 1..128 literals; 10xxxxxx: 3..66 repeats plus value;
 * 11xxxxxx: 3..66 backreference bytes plus nonzero block-local distance.
 */
static bool calendar_alpha_read(uint32_t start,uint32_t count,uint8_t *out) {
    if(!out || start>CALENDAR_ALPHA_BYTES || count>CALENDAR_ALPHA_BYTES-start)return false;
    while(count) {
        unsigned block=start/256u,skip=start%256u;
        unsigned expected=CALENDAR_ALPHA_BYTES-block*256u;if(expected>256u)expected=256u;
        uint32_t pos=calendar_alpha_blocks[block],end=calendar_alpha_blocks[block+1];
        if(pos>end || end>sizeof(calendar_alpha_packed))return false;
        uint8_t decoded[256];unsigned used=0;
        while(pos<end) {
            unsigned tag=calendar_alpha_packed[pos++];
            unsigned n=(tag<128u)?tag+1u:(tag&63u)+3u;
            if(n>expected-used)return false;
            if(tag<128u) {
                if(n>end-pos)return false;
                memcpy(decoded+used,calendar_alpha_packed+pos,n);pos+=n;
            } else {
                if(pos==end)return false;
                unsigned value=calendar_alpha_packed[pos++];
                if(tag<192u)memset(decoded+used,value,n);
                else {
                    if(!value || value>used)return false;
                    /* Forward byte copies deliberately support overlapping matches. */
                    for(unsigned i=0;i<n;i++)decoded[used+i]=decoded[used+i-value];
                }
            }
            used+=n;
        }
        if(used!=expected)return false;
        unsigned n=used-skip;if(n>count)n=count;
        memcpy(out,decoded+skip,n);out+=n;start+=n;count-=n;
    }
    return true;
}
static uint8_t calendar_alpha_glyph[CALENDAR_ALPHA_GLYPH_MAX];
