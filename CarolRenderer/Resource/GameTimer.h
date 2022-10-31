#pragma once
#include <string>
using std::wstring;

namespace Carol
{
	class GameTimer
	{
	public:
		GameTimer();

		float TotalTime()const;
		float DeltaTime()const;

		void Reset();
		void Start();
		void Stop();
		void Tick();

		wstring CalculateFrameStates(wstring& mainWindowCaption);

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


