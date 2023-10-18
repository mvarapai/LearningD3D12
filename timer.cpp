#include "Timer.h"

// Initializes member variables and gets amount of counts per second
Timer::Timer()
	: mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false), mStopTime(0)
{
	__int64 countsPerSec = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

float Timer::TotalTime() const
{
	if (mStopped)
	{
		return (float)(((mStopTime - mPausedTime) - mBaseTime)
			* mSecondsPerCount);
	}
	else
	{
		return (float)((mCurrTime - mPausedTime - mBaseTime)
			* mSecondsPerCount);
	}
}

// Time between frames
float Timer::DeltaTime() const
{
	return (float)mDeltaTime;
}

// Called once before the loop
void Timer::Reset()
{
	__int64 currTime = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

// Start the timer
void Timer::Start()
{
	__int64 startTime = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	if (mStopped)
	{
		// Accumulate paused time
		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;

		mStopTime = 0;
		mStopped = false;
	}

}

void Timer::Stop()
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		mStopTime = currTime;
		mStopped = true;
	}
}

// Call in the loop function
void Timer::Tick()
{
	// If stopped, do nothing
	if (mStopped) {
		mDeltaTime = 0;
		return;
	}

	// Get current time
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	// Delta time - time passed between frames in seconds
	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	// Set current time as previous for the next frame
	mPrevTime = mCurrTime;

	// Force non-negative value
	if (mDeltaTime < 0.0) {
		mDeltaTime = 0.0;
	}
}
