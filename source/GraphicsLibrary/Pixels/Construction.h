#ifndef __PIXELS_CONSTRUCTION_H__
#define __PIXELS_CONSTRUCTION_H__

#include "Functions.h"

#define RawRGB(r,g,b) ( (((uint32_t)(r))<<PixelRedShift)|(((uint32_t)(g))<<PixelGreenShift)|(((uint32_t)(b))<<PixelBlueShift)|BitMaskAt(PixelAlphaShift,PixelAlphaBits) )
#define RawRGBA(r,g,b,a) ( (((uint32_t)(r))<<PixelRedShift)|(((uint32_t)(g))<<PixelGreenShift)|(((uint32_t)(b))<<PixelBlueShift)|(((uint32_t)(a))<<PixelAlphaShift) )

#define UnclampedRGB(r,g,b) RawRGB( \
	((uint32_t)(r))>>(8-PixelRedBits), \
	((uint32_t)(g))>>(8-PixelGreenBits), \
	((uint32_t)(b))>>(8-PixelBlueBits))

#define UnclampedRGBA(r,g,b,a) RawRGBA( \
	((uint32_t)(r))>>(8-PixelRedBits), \
	((uint32_t)(g))>>(8-PixelGreenBits), \
	((uint32_t)(b))>>(8-PixelBlueBits), \
	((uint32_t)(a))>>(8-PixelAlphaBits))

#define RGB(r,g,b) UnclampedRGB(Clamp(r,0,0xff),Clamp(g,0,0xff),Clamp(b,0,0xff))

#define RGBA(r,g,b,a) UnclampedRGBA(Clamp(r,0,0xff),Clamp(g,0,0xff),Clamp(b,0,0xff),Clamp(a,0,0xff))

#endif
