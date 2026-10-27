#pragma once
#include <stdint.h>
/* Exact signed division recurrence for successive pixel-center numerators.
 * Positive denominator; numerator increases65536 per destination column.
 * Euclidean remainder permits branch/add stepping across zero, then restores
 * C truncation toward zero at observation. No pixel, frame or provider state. */
typedef struct {
    int64_t quotient;
    uint32_t remainder,denominator,step_quotient,step_remainder;
} native_alpha_axis;
static inline native_alpha_axis native_alpha_axis_begin(int64_t numerator,int denominator) {
    int64_t q=numerator/denominator,r=numerator%denominator;
    if(r<0){--q;r+=denominator;}
    return (native_alpha_axis){q,(uint32_t)r,(uint32_t)denominator,
        65536u/(uint32_t)denominator,65536u%(uint32_t)denominator};
}
static inline int64_t native_alpha_axis_next(native_alpha_axis *axis) {
    int64_t value=axis->quotient+(axis->quotient<0&&axis->remainder!=0);
    axis->quotient+=axis->step_quotient;
    axis->remainder+=axis->step_remainder;
    if(axis->remainder>=axis->denominator){axis->remainder-=axis->denominator;++axis->quotient;}
    return value;
}
