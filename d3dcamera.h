/*****************************************************************//**
 * \file   d3dcamera.h
 * \brief  Camera manager class
 * 
 * \author Mikalai Varapai
 * \date   November 2023
 *********************************************************************/

#pragma once

#include <DirectXMath.h>
#include <Windows.h>

#include "MathHelper.h"
#include "timer.h"

class Camera
{
public:
	Camera(DirectX::FXMVECTOR pos, float phi, float theta, Timer* timer)
	{
		mTimer = timer;

		mDirection = {
			cosf(mTheta) * sinf(mPhi),
			sinf(mTheta),
			cosf(mTheta) * cosf(mPhi),
			0.0f };

		mUp = {
			-cosf(mTheta) * sinf(mPhi),
			cosf(mTheta),
			-cosf(mTheta) * cosf(mPhi),
			0.0f };


		DirectX::XMStoreFloat4(&mPosition, pos);
		DirectX::XMVECTOR up = DirectX::XMLoadFloat4(&mUp);
		DirectX::XMVECTOR direction = DirectX::XMLoadFloat4(&mDirection);

		DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(pos, direction, up);
		DirectX::XMStoreFloat4x4(&mView, view);
	}

	Camera(Camera& rhs) = delete;
	Camera& operator=(Camera& rhs) = delete;

public:
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();

	POINT mLastMousePos = { };

	float Sensibility = 5.0f;

	DirectX::XMFLOAT4 mPosition = { };
private:
	// Private XMMatrixLookToLH parameters
	DirectX::XMFLOAT4 mDirection = { };
	DirectX::XMFLOAT4 mUp = { };

	float mPhi = 0.0f;
	float mTheta = 0.0f;

	float mSpeedZ = 0.0f;
	float mSpeedX = 0.0f;

	Timer* mTimer = nullptr;
public:
	void OnMouseMove(int mouseX, int mouseY)
	{
		float dPhi = DirectX::XMConvertToRadians(
			0.25f * static_cast<float>(mouseX - mLastMousePos.x));
		
		float dTheta = DirectX::XMConvertToRadians(
			0.25f * static_cast<float>(mouseY - mLastMousePos.y));

		mPhi += dPhi;
		mTheta -= dTheta;

		mTheta = MathHelper::Clamp(mTheta, -DirectX::XM_PIDIV4 + 0.1f, DirectX::XM_PIDIV4 - 0.1f);

		// Rewrite the view matrix based on new data
		mDirection = { 
			cosf(mTheta) * sinf(mPhi),
			sinf(mTheta),
			cosf(mTheta) * cosf(mPhi),
			0.0f };

		mUp = {
			- cosf(mTheta) * sinf(mPhi),
			cosf(mTheta),
			- cosf(mTheta) * cosf(mPhi),
			0.0f };
		mLastMousePos = { mouseX, mouseY };
	}
	void Update()
	{
		OnKeyDown();

		// Translate into DirectXMath vectors
		DirectX::XMVECTOR direction = DirectX::XMLoadFloat4(&mDirection);
		DirectX::XMVECTOR up = DirectX::XMLoadFloat4(&mUp);
		DirectX::XMVECTOR position = DirectX::XMLoadFloat4(&mPosition);
		DirectX::XMVECTOR left = DirectX::XMVector3Cross(direction, up);

		// Process camera movement
		
		// Clamp the speed value
		mSpeedX = MathHelper::Clamp(mSpeedX, -30.0f, 30.0f);
		mSpeedZ = MathHelper::Clamp(mSpeedZ, -30.0f, 30.0f);

		// Forward/Backward
		position = DirectX::XMVectorAdd(position,
			DirectX::XMVectorAdd(
				DirectX::XMVectorScale(direction, mSpeedZ * mTimer->DeltaTime()),
				DirectX::XMVectorScale(left, -mSpeedX * mTimer->DeltaTime())));

		// Return speed to equilibrium
		mSpeedX *= 0.95f;
		mSpeedZ *= 0.95f;

		DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(position, direction, up);

		DirectX::XMStoreFloat4x4(&mView, view);
		DirectX::XMStoreFloat4(&mPosition, position);
	}

	void OnKeyDown()
	{
		if (GetAsyncKeyState(0x57)) // W key
		{
			mSpeedZ += 1.0f;
		}
		if (GetAsyncKeyState(0x53)) // S key
		{
			mSpeedZ -= 1.0f;
		}
		if (GetAsyncKeyState(0x44)) // D key
		{
			mSpeedX += 1.0f;
		}
		if (GetAsyncKeyState(0x41)) // A key
		{
			mSpeedX -= 1.0f;
		}
	}
};
