#include "LinearColour.h"
#include <math.h>

static float linearTosRGB(float value);
static float sRGBToLinear(float value);
void LinearToOklab(float R, float G, float B, float *L, float *a, float *b);
void OklabToLinear(float L, float a, float b, float *R, float *G, float *B);

Pixel LinearRGB(float r, float g, float b) {
	return RGBf(linearTosRGB(r), linearTosRGB(g), linearTosRGB(b));
}

float ExtractLinearRed(Pixel c) {
	return sRGBToLinear(ExtractRedf(c));
}

float ExtractLinearGreen(Pixel c) {
	return sRGBToLinear(ExtractGreenf(c));
}

float ExtractLinearBlue(Pixel c) {
	return sRGBToLinear(ExtractBluef(c));
}

Pixel Oklab(float L, float a, float b) {
	float R, G, B;
	OklabToLinear(L, a, b, &R, &G, &B);
	return LinearRGB(R, G, B);
}

void ExtractOklab(Pixel c, float *L, float *a, float *b) {
	LinearToOklab(ExtractLinearRed(c), ExtractLinearGreen(c), ExtractLinearBlue(c), L, a, b);
}

static float linearTosRGB(float value) {
	float v = fmaxf(0, fminf(1, value));
	if(v <= 0.0031308) return v * 12.92;
	else return (1.055 * powf(v, 1 / 2.4) - 0.055);
}

static float sRGBToLinear(float value) {
	if(value <= 0.04045) return value / 12.92;
	else return powf((value + 0.055) / 1.055, 2.4);
}

void LinearToOklab(float R, float G, float B, float *L, float *a, float *b) {
	float l = 0.4122214708f * R + 0.5363325363f * G + 0.0514459929f * B;
	float m = 0.2119034982f * R + 0.6806995451f * G + 0.1073969566f * B;
	float s = 0.0883024619f * R + 0.2817188376f * G + 0.6299787005f * B;

	float l1_3 = cbrtf(l);
	float m1_3 = cbrtf(m);
	float s1_3 = cbrtf(s);

	*L = 0.2104542553f * l1_3 + 0.7936177850f * m1_3 - 0.0040720468f * s1_3;
	*a = 1.9779984951f * l1_3 - 2.4285922050f * m1_3 + 0.4505937099f * s1_3;
	*b = 0.0259040371f * l1_3 + 0.7827717662f * m1_3 - 0.8086757660f * s1_3;
}

void OklabToLinear(float L, float a, float b, float *R, float *G, float *B) {
	float l1_3 = L + 0.3963377774f * a + 0.2158037573f * b;
	float m1_3 = L - 0.1055613458f * a - 0.0638541728f * b;
	float s1_3 = L - 0.0894841775f * a - 1.2914855480f * b;

	float l = l1_3 * l1_3 * l1_3;
	float m = m1_3 * m1_3 * m1_3;
	float s = s1_3 * s1_3 * s1_3;

	*R = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
	*G = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
	*B = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}
