// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include <HAL/Event.h>
#include <Templates/UniquePtr.h>
#include <Chaos/Array.h>
#include <Templates/Atomic.h>
#include <Misc/SpinLock.h>
#include <Misc/SpinLock.h>
#include <Misc/QueuedThreadPool.h>
#include <Containers/Queue.h>
#include <Containers/LockFreeList.h>

#include "TestThreadingGameMode.generated.h"


class FMyWorker;

UCLASS(minimalapi)
class ATestThreadingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATestThreadingGameMode();

	void StartPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void Tick(float DeltaSeconds) override;

	void OnZPressed();
	void OnXPressed();

	void OnCPressed();
	void OnVPressed();
	void OnBPressed();
	void OnNPressed();
	void OnMPressed();
	void OnHPressed();
	void OnJPressed();
	void OnKPressed();
	void OnLPressed();


	void OnWorkerTick(FMyWorker* InWorker);
	void OnWorkerDone(FMyWorker* InWorker);

	UE::FSpinLock m_WorkersSpinLock;

	TAtomic<uint32> m_NumMyWorkers;
	TArray<TUniquePtr<FMyWorker>> m_Workers;
	TQueue<TUniquePtr<FMyWorker>> m_DoneWorkers;

	TLockFreePointerListUnordered<TUniquePtr<FMyWorker>, PLATFORM_CACHE_LINE_SIZE> m_MyWorkersLockFreeList;

	TAtomic<uint32> m_NumTicks = 0;
	uint32 m_WorkerTicksMutexProtected = 0;
	uint32 m_WorkerTickSpinLockProtected = 0;


	FCriticalSection m_WorkersCriticalSection;
	UE::FSpinLock m_WorkerTickSpinLock;

	TAtomic<uint32> m_QueueSize = 0;
	TQueue<uint32, EQueueMode::Mpsc> m_Ids;;
	//FQueuedThreadPool m_QueuedThreadPool;

	//FCriticalSection
	//FScopeLock
	//FThreadSafeCounter
	//FSpinLockcan 
	//threadsFScopedEvent

	//TLockFreePointerList
	//TQueue
	//MPSCTDisruptor
	//ParallelFor
};



//FNonAbandonableTask is the name of the class I've located from the source code of the engine*/
class FTestNonAbandonableTask : public FNonAbandonableTask
{
	int32 MaxPrime;

public:
	/*Default constructor*/
	FTestNonAbandonableTask(int32 MaxPrime)
	{
		this->MaxPrime = MaxPrime;
	}

	/*This function is needed from the API of the engine.
	My guess is that it provides necessary information
	about the thread that we occupy and the progress of our task*/
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTestNonAbandonableTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	/*This function is executed when we tell our task to execute*/
	void DoWork()
	{
		//ThreadingTest::CalculatePrimeNumbers(MaxPrime);



		GLog->Log("--------------------------------------------------------------------");
		GLog->Log("End of prime numbers calculation on background thread");
		GLog->Log("--------------------------------------------------------------------");
	}
};
// 
// class FTestRunnable : public FRunnable 
// {
// protected:
// 	/** The event that tells the thread there is work to do. */
// 	FEvent* DoWorkEvent;
// 	/** The work this thread is doing. */
// 	IQueuedWork* volatile QueuedWork;
// 	/** The pool this thread belongs to. */
// 	class FQueuedThreadPool* OwningThreadPool;
// 	/** My Thread  */
// 	FRunnableThread* Thread;
// 
// 	virtual uint32 Run() override {
// 		while (!TimeToDie.Load(EMemoryOrder::Relaxed)) {
// 			// We need to wait for shorter amount of time
// 			bool bContinueWaiting = true;
// 			while (bContinueWaiting) {
// 				// Wait for some work to do
// 				bContinueWaiting = !DoWorkEvent->Wait(10);
// 			}
// 
// 			IQueuedWork* LocalQueuedWork = QueuedWork;
// 			QueuedWork = nullptr;
// 			FPlatformMisc::MemoryBarrier();
// 			while (LocalQueuedWork) {
// 				// Tell the object to do the work
// 				LocalQueuedWork->DoThreadedWork();
// 				// Let the object cleanup before we remove our ref to it
// 				LocalQueuedWork = OwningThreadPool->ReturnToPoolOrGetNextJob(this);
// 			}
// 		}
// 		return 0;
// 	}
// 	...
// 		void DoWork(IQueuedWork* InQueuedWork) {
// 		// Tell the thread the work to be done
// 		QueuedWork = InQueuedWork;
// 		FPlatformMisc::MemoryBarrier();
// 		// Tell the thread to wake up and do its job
// 		DoWorkEvent->Trigger();
// 	}
// };



class FMyWorker : public FRunnable
{
public:

	DECLARE_DELEGATE(FOnTick);
	DECLARE_DELEGATE(FOnDone);

	// Constructor, create the thread by calling this
	FMyWorker(uint32 InId, uint32 NumTicks);

	// Destructor
	virtual ~FMyWorker() override;

	void Start();
	void StopNow();

	// Overriden from FRunnable
	// Do not call these functions youself, that will happen automatically
	bool Init() override; // Do your setup here, allocate memory, ect.
	uint32 Run() override; // Main data processing happens here
	void Stop() override; // Clean up any memory you allocated here


	FOnTick m_OnTickDelegate;
	FOnDone m_OnDoneDelegate;

//private:

	// Thread handle. Control the thread using this, with operators like Kill and Suspend
	FRunnableThread* m_Thread = nullptr;
	FEvent* m_StopEvent = nullptr;

	uint32 m_Id = 0;
	uint32 m_NumTicks = 0;
	uint32 m_Ticks = 0;

	// Used to know when the thread should exit, changed in Stop(), read in Run()
	bool m_bStopThread = false;

	FColor m_Color;

};