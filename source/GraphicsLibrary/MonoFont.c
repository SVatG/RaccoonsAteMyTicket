#include "MonoFont.h"
#include "Drawing.h"
#include "GenericDrawing.h"

static inline void GenericDrawCharacter(Bitmap *bitmap,const Font *font,int x,int y,
Pixel col,CompositionMode comp,int c,GenericDrawHorizontalLineFunction *hlinefunc);

int KerningForMonoFontCharacters(const Font *font,int c,int prev)
{
	const MonoFont *self=(const MonoFont *)font;

	if(c<self->firstglyph || c>self->lastglyph) return 0;
	if(!self->glyphs[c-self->firstglyph]) return 0;

	const uint8_t *data=self->glyphs[c-self->firstglyph];
	return (int8_t)data[0];
}

int SpacingForMonoFontCharacter(const Font *font,int c)
{
	const MonoFont *self=(const MonoFont *)font;

	if(c<self->firstglyph || c>self->lastglyph) return 0;
	if(!self->glyphs[c-self->firstglyph]) return 0;

	const uint8_t *data=self->glyphs[c-self->firstglyph];
	return data[1];
}

void DrawMonoFontCharacter(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,int c)
{
	GenericDrawCharacter(bitmap,font,x,y,col,NULL,c,DrawHorizontalLineFunction);
}

void CompositeMonoFontCharacter(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,CompositionMode comp,int c)
{
	GenericDrawCharacter(bitmap,font,x,y,col,comp,c,CompositeHorizontalLineFunction);
}

static inline void GenericDrawCharacter(Bitmap *bitmap,const Font *font,int x,int y,
Pixel col,CompositionMode comp,int c,GenericDrawHorizontalLineFunction *hlinefunc)
{
	const MonoFont *self=(const MonoFont *)font;

	if(c<self->firstglyph || c>self->lastglyph) return;
	if(!self->glyphs[c-self->firstglyph]) return;

	const uint8_t *data=self->glyphs[c-self->firstglyph];
	int width=data[2];
	int height=data[3];
	const uint8_t *spans=&data[4];

	for(int dy=0;dy<height;dy++)
	{
		int dx=0;
		while(dx<width)
		{
			dx+=*spans++;
			if(dx>=width) break;

			int length=*spans++;
			GenericDrawHorizontalLine(bitmap,x+dx,y+dy,length,col,comp,hlinefunc);
			dx+=length;
		}
	}
}

