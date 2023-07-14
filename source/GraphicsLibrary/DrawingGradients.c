#include "Drawing.h"
#include "LinearColour.h"

void DrawVerticalGradientInLinearRGB(Bitmap *bitmap, int x, int y, int width, int height, Pixel from, Pixel to, EasingFunction easing) {
	float r1 = ExtractLinearRed(from);
	float g1 = ExtractLinearGreen(from);
	float b1 = ExtractLinearBlue(from);
	float r2 = ExtractLinearRed(to);
	float g2 = ExtractLinearGreen(to);
	float b2 = ExtractLinearBlue(to);

	for(int i = 0; i < height; i++) {
		float t = easing((float)i / (float)(height - 1));
		float r = r1 + (r2 - r1) * t;
		float g = g1 + (g2 - g1) * t;
		float b = b1 + (b2 - b1) * t;
		DrawHorizontalLine(bitmap, x, y + i, width, LinearRGB(r, g, b));
	}
}

void DrawVerticalGradientInOklab(Bitmap *bitmap, int x, int y, int width, int height, Pixel from, Pixel to, EasingFunction easing) {
	float L1, a1, b1;
	float L2, a2, b2;
	ExtractOklab(from, &L1, &a1, &b1);
	ExtractOklab(to, &L2, &a2, &b2);

	for(int i = 0; i < height; i++) {
		float t = easing((float)i / (float)(height - 1));
		float L = L1 + (L2 - L1) * t;
		float a = a1 + (a2 - a1) * t;
		float b = b1 + (b2 - b1) * t;
		DrawHorizontalLine(bitmap, x, y + i, width, Oklab(L, a, b));
	}
}

void DrawHorizontalGradientInLinearRGB(Bitmap *bitmap, int x, int y, int width, int height, Pixel from, Pixel to, EasingFunction easing) {
	float r1 = ExtractLinearRed(from);
	float g1 = ExtractLinearGreen(from);
	float b1 = ExtractLinearBlue(from);
	float r2 = ExtractLinearRed(to);
	float g2 = ExtractLinearGreen(to);
	float b2 = ExtractLinearBlue(to);

	for(int i = 0; i < width; i++) {
		float t = easing((float)i / (float)(width - 1));
		float r = r1 + (r2 - r1) * t;
		float g = g1 + (g2 - g1) * t;
		float b = b1 + (b2 - b1) * t;
		DrawVerticalLine(bitmap, x + i, y, height, LinearRGB(r, g, b));
	}
}

void DrawHorizontalGradientInOklab(Bitmap *bitmap, int x, int y, int width, int height, Pixel from, Pixel to, EasingFunction easing) {
	float L1, a1, b1;
	float L2, a2, b2;
	ExtractOklab(from, &L1, &a1, &b1);
	ExtractOklab(to, &L2, &a2, &b2);

	for(int i = 0; i < width; i++) {
		float t = easing((float)i / (float)(width - 1));
		float L = L1 + (L2 - L1) * t;
		float a = a1 + (a2 - a1) * t;
		float b = b1 + (b2 - b1) * t;
		DrawVerticalLine(bitmap, x + i, y, height, Oklab(L, a, b));
	}
}

float LinearEasing(float x) {
	return x;
}

float SmoothStepEasing(float x) {
	return x * x * (3 - 2 * x);
}
