#ifndef __HELPERS_RLE_BITMAP_LOADER_H__
#define __HELPERS_RLE_BITMAP_LOADER_H__

#include "RLEBitmapAllocator.h"
#include "BitmapLoader.h"

static RLEBitmap *AllocateRLEBitmapFromContentsOfPNGFile(const char *filename)
{
	Bitmap *bitmap=AllocateBitmapWithContentsOfPNGFile(filename);
	if(!bitmap) return NULL;

	RLEBitmap *self=AllocateRLEBitmapFromBitmap(bitmap);

	FreeBitmap(bitmap);

	return self;
}

#endif

