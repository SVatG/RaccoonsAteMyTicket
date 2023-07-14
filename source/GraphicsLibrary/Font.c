#include "Font.h"

int WidthOfSimpleString(const Font *font,const void *str)
{
	const uint8_t *cstring=str;
	uint8_t prev=0;
	int len=0;
	while(*cstring)
	{
		int c=*cstring++&0xff;
		int kerning=KerningForCharacters(font,c,prev);
		int spacing=SpacingForCharacter(font,c);
		len+=kerning+spacing;
		prev=c;
	}
	return len;
}

void DrawSimpleString(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,const void *str)
{
	const uint8_t *cstring=str;
	uint8_t prev=0;
	while(*cstring)
	{
		int c=*cstring++&0xff;
		int kerning=KerningForCharacters(font,c,prev);
		int spacing=SpacingForCharacter(font,c);
		DrawCharacter(bitmap,font,x+kerning,y,col,c);
		x+=kerning+spacing;
		prev=c;
	}
}

void CompositeSimpleString(Bitmap *bitmap,const Font *font,int x,int y,Pixel col,CompositionMode comp,const void *str)
{
	const uint8_t *cstring=str;
	uint8_t prev=0;
	while(*cstring)
	{
		int c=*cstring++&0xff;
		int kerning=KerningForCharacters(font,c,prev);
		int spacing=SpacingForCharacter(font,c);
		CompositeCharacter(bitmap,font,x+kerning,y,col,comp,c);
		x+=kerning+spacing;
		prev=c;
	}
}
