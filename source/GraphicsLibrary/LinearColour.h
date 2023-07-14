#ifndef __LINEAR_COLOUR_H__
#define __LINEAR_COLOUR_H__

#include "Pixels.h"

Pixel LinearRGB(float r, float g, float b);
float ExtractLinearRed(Pixel c);
float ExtractLinearGreen(Pixel c);
float ExtractLinearBlue(Pixel c);

Pixel Oklab(float L, float a, float b);
void ExtractOklab(Pixel c, float *L, float *a, float *b);

#endif
