#ifndef __PIXELS_H__
#define __PIXELS_H__

#include <stdint.h>
#include <stdbool.h>

#if defined(PremultipliedABGR32Pixels)
#include "Pixels/PremultipliedABGR32.h"
#elif defined(PremultipliedARGB32Pixels)
#include "Pixels/PremultipliedARGB32.h"
#elif defined(PremultipliedBGRA32Pixels)
#include "Pixels/PremultipliedBGRA32.h"
#elif defined(PremultipliedRGBA32Pixels)
#include "Pixels/PremultipliedRGBA32.h"

#elif defined(ABGR32Pixels)
#include "Pixels/ABGR32.h"
#elif defined(ARGB32Pixels)
#include "Pixels/ARGB32.h"
#elif defined(BGRA32Pixels)
#include "Pixels/BGRA32.h"
#elif defined(RGBA32Pixels)
#include "Pixels/RGBA32.h"

#elif defined(ARGB16Pixels)
#include "Pixels/ARGB16.h"
#elif defined(RGB16Pixels)
#include "Pixels/RGB16.h"

#elif defined(RGB8Pixels)
#include "Pixels/RGB8.h"
#elif defined(IndexedPixels)
#include "Pixels/Indexed.h"

#else // Default to BGRA32 pixels.
#include "Pixels/BGRA32.h"

#endif

#define RGBf(r,g,b) RGB((r)*255,(g)*255,(b)*255)
#define RGBAf(r,g,b,a) RGBA((r)*255,(g)*255,(b)*255,(a)*255)
#define RGBHex(hex) RGB(((hex)>>16)&0xff,((hex)>>8)&0xff,(hex)&0xff)
#define White(l) RGB(l,l,l)
#define Whitef(l) Grey(l*255)
#define Grey(v) White(v)

#define ExtractRedf(c) (ExtractRed(c) / 255.f)
#define ExtractGreenf(c) (ExtractGreen(c) / 255.f)
#define ExtractBluef(c) (ExtractBlue(c) / 255.f)

#endif
