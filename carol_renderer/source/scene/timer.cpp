#include <scene/timer.h>
#include <windows.h>

Carol::Timer::Timer()
{
    uint64_t countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);

    mSecondsPerCount = 1.0 / countsPerSec;
}

float Carol::Timer::TotalTime() const
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

float Carol::Timer::DeltaTime() const
{
    return static_cast<float>(mDeltaTime);
}

void Carol::Timer::Reset()
{
    uint64_t currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

    mBaseTime = currTime;
    mPrevTime = currTime;
    mStopTime = 0;
    mStopped = false;
}

void Carol::Timer::Start()
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

void Carol::Timer::Stop()
{
    if (!mStopped)
    {   
        uint64_t stopTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&stopTime);
        mStopped = true;

        mStopTime = stopTime;
    }
}

void Carol::Timer::Tick()
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

std::wstring Carol::Timer::CalculateFrameStates(std::wstring_view mainWindowCaption)
{
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    if ((TotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = (float)frameCnt;
        float mspf = 1000.0f / fps;

        std::wstring fpsStr = std::to_wstring(fps);
        std::wstring mspfStr = std::to_wstring(mspf);

        std::wstring windowText = std::wstring(mainWindowCaption) +
            L"    fps: " + fpsStr +
            L"   mspf: " + mspfStr;

        frameCnt = 0;
        timeElapsed += 1.0f;

        return windowText;
    }

    return std::wstring();
}
