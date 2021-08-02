#ifndef DBC_OS_MATH_H
#define DBC_OS_MATH_H

#include <iostream>
#include <string>

#define ABS(x) ( (x)>0?(x):-(x) )
#define LIMIT( x,min,max ) ( (x) < (min)  ? (min) : ( (x) > (max) ? (max) : (x) ) )
#define _MIN(a, b) ((a) < (b) ? (a) : (b))
#define _MAX(a, b) ((a) > (b) ? (a) : (b))

#define my_pow(a) ((a)*(a))
#define safe_div(numerator,denominator,safe_value) ((denominator == 0) ? (safe_value) : ((numerator)/(denominator)))

float my_sqrt(float number);

#endif
