#ifndef __MONO_FONT_H__
#define __MONO_FONT_H__

#include "Font.h"

typedef struct MonoFont
{
	Font font;
	int firstglyph,lastglyph;
	uint8_t *glyphs[0];
} MonoFont;

int KerningForMonoFontCharacters(const Font *font,int c,int prev);
int SpacingForMonoFontCharacter(const Font *font,int c);
void DrawMonoFontCharacter(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,int c);
void CompositeMonoFontCharacter(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,CompositionMode comp,int c);

#endif
