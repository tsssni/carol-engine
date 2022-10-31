#include "GameTimer.h"
#include <windows.h>

Carol::GameTimer::GameTimer()
{
    uint64_t countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);

    mSecondsPerCount = 1.0 / countsPerSec;
}

float Carol::GameTimer::TotalTime() const
{
    if (mStopped)
    {
        return static_cast<float>(((mStopTime - mPausedTime) - mBaseTime)) * mSecondsPerCount;
    }
    else
    {
        return static_cast<float>(((mCurrTime - mPausedTime) - mBaseTime)) * mSecondsPerCount;
    }
}

float Carol::GameTimer::DeltaTime() const
{
    return static_cast<float>(mDeltaTime);
}

void Carol::GameTimer::Reset()
{
    uint64_t currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

    mBaseTime = currTime;
    mPrevTime = currTime;
    mStopTime = 0;
    mStopped = false;
}

void Carol::GameTimer::Start()
{
    if (mStopped)
    {
        uint64_t startTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
        mStopped = false;

        mPausedTime += startTime - mStopTime;
        mPrevTime = startTime;
        mStopTime = 0;
    }
}

void Carol::GameTimer::Stop()
{
    

    if (!mStopped)
    {   
        uint64_t stopTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&stopTime);
        mStopped = true;

        mStopTime = stopTime;
    }
}

void Carol::GameTimer::Tick()
{
    if (mStopped)
    {
        mDeltaTime = 0.0f;
        return;
    }

    uint64_t currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    
    mCurrTime = currTime;
    mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
    mPrevTime = mCurrTime;

    if (mDeltaTime < 0.0f)
    {
        mDeltaTime = 0.0f;
    }
}

wstring Carol::GameTimer::CalculateFrameStates(wstring& mainWindowCaption)
{
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    if ((TotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = (float)frameCnt;
        float mspf = 1000.0f / fps;

        wstring fpsStr = std::to_wstring(fps);
        wstring mspfStr = std::to_wstring(mspf);

        wstring windowText = mainWindowCaption +
            L"    fps: " + fpsStr +
            L"   mspf: " + mspfStr;

        frameCnt = 0;
        timeElapsed += 1.0f;

        return windowText;
    }

    return wstring();
}
