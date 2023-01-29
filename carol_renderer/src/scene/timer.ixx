export module carol.renderer.scene.timer;
import <string>;
import <string_view>;
import <windows.h>;

namespace Carol
{
    using std::wstring;
    using std::wstring_view;

    export class Timer
	{
	public:
		Timer()
        {
            uint64_t countsPerSec;
            QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);

            mSecondsPerCount = 1.0 / countsPerSec;
        }

		float TotalTime()const
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

		float DeltaTime()const
        {
            return static_cast<float>(mDeltaTime);
        }

		void Reset()
        {
            uint64_t currTime;
            QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

            mBaseTime = currTime;
            mPrevTime = currTime;
            mStopTime = 0;
            mStopped = false;
        }

		void Start()
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

		void Stop()
        {
            if (!mStopped)
            {   
                uint64_t stopTime;
                QueryPerformanceCounter((LARGE_INTEGER*)&stopTime);
                mStopped = true;

                mStopTime = stopTime;
            }
        }

		void Tick()
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

		wstring CalculateFrameStates(wstring_view mainWindowCaption)
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

                wstring windowText = wstring(mainWindowCaption) +
                    L"    fps: " + fpsStr +
                    L"   mspf: " + mspfStr;

                frameCnt = 0;
                timeElapsed += 1.0f;

                return windowText;
            }

            return wstring();
        }

	private:
		double mSecondsPerCount = 0.0f;
		double mDeltaTime = -1.0f;

		uint64_t mBaseTime = 0;
		uint64_t mPausedTime = 0;
		uint64_t mStopTime = 0;
		uint64_t mPrevTime = 0;
		uint64_t mCurrTime = 0;

		bool mStopped = false;
	};
}