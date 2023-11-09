#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>

class MathHelper
{
public:
	static DirectX::XMFLOAT4X4 Identity4x4()
	{
		static DirectX::XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
		return I;
	}

	static const float Pi;

	static float Clamp(float value, float min, float max);

	static float TerrainNoise(float x, float z)
	{
		return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
	}
};