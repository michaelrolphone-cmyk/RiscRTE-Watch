#pragma once
#include <stddef.h>
#include <stdint.h>
/* Lossless hard wrapping: every byte is consumed once; only source newline
 * bytes occupy a line break. UI controls are deliberately outside report text. */
static inline uint32_t crash_text_line(const char *text,uint32_t size,uint32_t at,char out[29]) {
 unsigned n=0;while(at<size && text[at]!='\n' && n<28)out[n++]=text[at++];
 out[n]=0;if(at<size && text[at]=='\n')++at;return at;
}
static inline uint32_t crash_text_page(const char *text,uint32_t size,uint32_t at,unsigned rows) {
 char line[29];for(unsigned i=0;i<rows && at<size;++i)at=crash_text_line(text,size,at,line);return at;
}
