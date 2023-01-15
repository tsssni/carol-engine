#pragma once
#include <string>

namespace Carol
{
	class Timer
	{
	public:
		Timer();

		float TotalTime()const;
		float DeltaTime()const;

		void Reset();
		void Start();
		void Stop();
		void Tick();

		std::wstring CalculateFrameStates(std::wstring& mainWindowCaption);

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


