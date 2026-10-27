#include <assert.h>
#include <stdio.h>
#include NOVA_SOURCE
static uint16_t oracle(uint16_t old,uint16_t color,unsigned alpha){
 unsigned out=0;const unsigned shift[]={0,5,11},mask[]={31,63,31};
 for(unsigned n=0;n<3;n++){unsigned a=(old>>shift[n])&mask[n],b=(color>>shift[n])&mask[n];
 /* Independent nearest integer rational: quotient plus remainder threshold. */
 unsigned sum=a*(255-alpha)+b*alpha,q=sum/255,r=sum%255;
 out|=(q+(r>=128))<<shift[n];}return (uint16_t)out;
}
int main(void){unsigned long cases=0;uint32_t rng=0x172bab23;
 for(unsigned c=0;c<65536;c++)for(unsigned a=0;a<256;a++){uint8_t p[]={c,c>>8};blend(p,(uint16_t)c,a);assert((p[0]|((unsigned)p[1]<<8))==c);assert(oracle(c,c,a)==c);cases++;}
 for(unsigned i=0;i<2000000;i++){rng=rng*1664525u+1013904223u;uint16_t old=rng>>16;rng=rng*1664525u+1013904223u;uint16_t c=rng>>16;unsigned a=rng&255;uint8_t p[]={old,old>>8};blend(p,c,a);assert((p[0]|((unsigned)p[1]<<8))==oracle(old,c,a));cases++;}
 printf("PASS %lu exact identity/random oracle cases\n",cases);return 0;}
