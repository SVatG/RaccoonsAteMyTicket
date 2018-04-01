#include "ColourFont.h"
#include "Drawing.h"

int SpacingForColourFontCharacter(const Font *font,int c)
{
	const ColourFont *self=(const ColourFont *)font;

	if(c<self->firstglyph || c>self->lastglyph) return 0;

	return self->glyphs[c-self->firstglyph].spacing;
}

void DrawColourFontCharacter(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,int c)
{
	// printf("%d\n", c);
	const ColourFont *self=(const ColourFont *)font;

	if(c<self->firstglyph || c>self->lastglyph) return;
	// printf("%d %d %d %d\n", c, self->firstglyph, self->lastglyph, self->glyphs[c-self->firstglyph].spacing);

	const RLEBitmap *glyph=self->glyphs[c-self->firstglyph].rle;
	DrawRLEBitmap(bitmap,glyph,x,y);
}

void CompositeColourFontCharacter(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,CompositionMode comp,int c)
{
	const ColourFont *self=(const ColourFont *)font;

	if(c<self->firstglyph || c>self->lastglyph) return;

	const RLEBitmap *glyph=self->glyphs[c-self->firstglyph].rle;
	CompositeRLEBitmap(bitmap,glyph,x,y,comp);
}

