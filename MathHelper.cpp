#include "MathHelper.h"

const float MathHelper::Pi = 3.1415926535f;

float MathHelper::Clamp(float value, float min, float max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}