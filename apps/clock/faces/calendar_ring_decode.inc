/* Exact D4 symmetry of the generated 240x240 antialiased calendar rings.
 * Angles use 128 units/degree. Pixel centers are mirrored around 119.5.
 * One octant contains x >= y >= 120. Diagonal swaps are omitted so each
 * output pixel is blended exactly once. No heap or persistent workspace.
 */
static bool fc_ring_expand(const fc_ring_pixel *base,unsigned transform,fc_ring_pixel *out) {
    if(!base || !out || transform>=8u)return false;
    unsigned x=base->pixel%240u,y=base->pixel/240u;
    if(y<120u || x<y || x>=240u)return false;
    int angle=base->angle;
    if(angle<11520 || angle>17280)return false;
    if(transform&1u) {
        if(x==y)return false;
        unsigned tmp=x;x=y;y=tmp;angle=34560-angle;
    }
    if(transform&2u) {x=239u-x;angle=46080-angle;}
    if(transform&4u) {y=239u-y;angle=23040-angle;}
    if(angle<0)angle+=46080;
    if(angle>=46080)angle-=46080;
    out->pixel=(uint16_t)(y*240u+x);out->angle=(uint16_t)angle;out->alpha=base->alpha;
    return true;
}
