// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestThreadingGameMode.h"
#include "TestThreadingCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include <Templates/Function.h>
#include <Misc/ScopeLock.h>
#include <Misc/SpinLock.h>
#include <DistributedBuildInterface/Public/DistributedBuildControllerInterface.h>
#include <Tasks/Task.h>
#include <HAL/ThreadManager.h>

PRAGMA_DISABLE_OPTIMIZATION

ATestThreadingGameMode::ATestThreadingGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ATestThreadingGameMode::StartPlay()
{
	Super::StartPlay();


	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	UInputComponent* PlayerControllerInputComponent = PlayerController->InputComponent;

	if (PlayerControllerInputComponent)
	{
		PlayerControllerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &ThisClass::OnXPressed);
		PlayerControllerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ThisClass::OnZPressed);
		PlayerControllerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ThisClass::OnCPressed);
		PlayerControllerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ThisClass::OnVPressed);
		PlayerControllerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &ThisClass::OnBPressed);
		PlayerControllerInputComponent->BindKey(EKeys::N, IE_Pressed, this, &ThisClass::OnNPressed);
		PlayerControllerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ThisClass::OnMPressed);
		PlayerControllerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &ThisClass::OnHPressed);
		PlayerControllerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ThisClass::OnJPressed);
		PlayerControllerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &ThisClass::OnKPressed);
		PlayerControllerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &ThisClass::OnLPressed);
	}
}



void ATestThreadingGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TUniquePtr<FMyWorker>& MyWorker : m_Workers)
	{
		MyWorker->StopNow();
	}

	m_Workers.Empty();
	m_NumMyWorkers = 0;

	Super::EndPlay(EndPlayReason);
}

void ATestThreadingGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GEngine)
	{
		const uint32 NumWorkers = m_NumMyWorkers;
		const uint32 NumTicks = m_NumTicks;
		GEngine->AddOnScreenDebugMessage(123, 15.0f, FColor::Red, FString::Printf(TEXT("num workers %u\nnum ticks %u"), NumWorkers, NumTicks));
	}

	TUniquePtr<FMyWorker> DoneWorker;
	while (m_DoneWorkers.Dequeue(DoneWorker))
	{
		DoneWorker.Reset();
	}

}

void ATestThreadingGameMode::OnZPressed()
{
	static uint32 IdGenerator = 0;
	IdGenerator++;

	TUniquePtr<FMyWorker> MyWorker = MakeUnique<FMyWorker>(IdGenerator, (uint32)FMath::RandRange(8, 12));

	MyWorker->m_OnTickDelegate.BindWeakLambda(this, [this, WorkerPtr = MyWorker.Get()]() { OnWorkerTick(WorkerPtr); });
	MyWorker->m_OnDoneDelegate.BindWeakLambda(this, [this, WorkerPtr = MyWorker.Get()]() { OnWorkerDone(WorkerPtr); });
	MyWorker->Start();

	{
		FScopeLock Lock(&m_WorkersCriticalSection);
		//UE::TScopeLock<UE::FSpinLock> ScopeLock(m_WorkersSpinLock);
		m_Workers.Add(MoveTemp(MyWorker));
		m_NumMyWorkers++;
	}
}

void ATestThreadingGameMode::OnXPressed()
{
	if (m_NumMyWorkers != 0)
	{
		TUniquePtr<FMyWorker> MyWorker;

		{
			FScopeLock Lock(&m_WorkersCriticalSection);
			//UE::TScopeLock<UE::FSpinLock> ScopeLock(m_WorkersSpinLock);
			MyWorker = MoveTemp(m_Workers[0]);
			m_Workers.RemoveAt(0);
			m_NumMyWorkers--;
		}

		MyWorker->m_OnDoneDelegate.Unbind();
		MyWorker->StopNow();
		MyWorker = nullptr;
	}
}

void ATestThreadingGameMode::OnCPressed()
{
	TFunction<uint32()> Task = []()
	{
		FPlatformProcess::Sleep(2.0f);
		return 123;
	};

	TFunction<uint32(const TFuture<uint32>&)> ThenTask = [](const TFuture<uint32>& Arg)
	{
		FPlatformProcess::Sleep(2.0f);
		return Arg.Get() + 456;
	};

	TFunction<void()> TaskDone = []()
	{
		UE_LOG(LogTemp, Warning, TEXT("Task done"));
	};

	TFuture<uint32> Result = Async(EAsyncExecution::Thread, Task, TaskDone).Then(ThenTask);


	uint32 sum = Result.Get() + 12;

	//if(Result.Get())

// 	while (!Result.IsReady())
// 	{
// 		if (GEngine)
// 			GEngine->AddOnScreenDebugMessage(1000 + (int32)m_Id, 1.f, m_Color, FString::Printf(TEXT("Waitinf for result"));
// 	}

}

struct FTestGraphTask1
{

	FTestGraphTask1(ATestThreadingGameMode* GameMode, uint32 NumTicks) :m_GameMode(GameMode), m_numTicks(NumTicks) {};

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FTestGraphTask1, STATGROUP_TaskGraphTasks); }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::Type::AnyThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FPlatformProcess::Sleep(2.0f);
	}


	ATestThreadingGameMode* m_GameMode;
	uint32 m_numTicks;

};

void ATestThreadingGameMode::OnVPressed()
{

	FGraphEventArray Prerequisites;

	FGraphEventRef Task = TGraphTask<FTestGraphTask1>::CreateTask(&Prerequisites).ConstructAndDispatchWhenReady(this, 10);

}

void ATestThreadingGameMode::OnBPressed()
{
	UE::Tasks::FTask TestTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, []()
		{
			ParallelFor(32, [](uint32 Index)
				{
					const FString& ThreadName = FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId());
					if (GEngine)
						GEngine->AddOnScreenDebugMessage(2000+ (uint64)Index, 2.f, FColor::Green, FString::Printf(TEXT("ParallelFor %u %s"), Index, *ThreadName));
				});
		}
		);
}

void ATestThreadingGameMode::OnNPressed()
{

}

void ATestThreadingGameMode::OnMPressed()
{

}

void ATestThreadingGameMode::OnHPressed()
{

}

void ATestThreadingGameMode::OnJPressed()
{

}

void ATestThreadingGameMode::OnKPressed()
{

}

void ATestThreadingGameMode::OnLPressed()
{

}

void ATestThreadingGameMode::OnWorkerTick(FMyWorker* InWorker)
{
	m_NumTicks++;

// 	{
// 		FScopeLock MutexLock(&m_WorkersCriticalSection);
// 		m_WorkerTicksMutexProtected++;
// 	}
// 
// 
// 	{
// 		UE::TScopeLock<UE::FSpinLock> ScopeLock(m_WorkerTickSpinLock);
// 		m_WorkerTickSpinLockProtected++;
// 	}

	m_QueueSize++;
	m_Ids.Enqueue(InWorker->m_Id);

	constexpr uint32 maxQueueSize = 10;
	if (m_QueueSize > maxQueueSize)
	{
		for (uint32 i = 0; i < maxQueueSize; ++i)
		{
			uint32 lastIds;
			m_Ids.Dequeue(lastIds);
		}

		m_QueueSize = 0;
	}

	//m_Ids.Dequeue()
}

void ATestThreadingGameMode::OnWorkerDone(FMyWorker* InWorker)
{
	TUniquePtr<FMyWorker> Worker;

	{
		FScopeLock Lock(&m_WorkersCriticalSection);

		const uint32 Found = m_Workers.IndexOfByPredicate([InWorker](const TUniquePtr<FMyWorker>& Worker) { return Worker.Get() == InWorker; });
		check(Found != INDEX_NONE);
		Worker = MoveTemp(m_Workers[Found]);
		m_Workers.RemoveAt(Found);
	}

	m_DoneWorkers.Enqueue(MoveTemp(Worker));
	m_NumMyWorkers--;
}

//////////////////////////////////////////////////////////////////////////

FMyWorker::FMyWorker(uint32 InId, uint32 NumTicks)
	: m_Id(InId)
	, m_NumTicks(NumTicks)
{

}


FMyWorker::~FMyWorker()
{
	if (m_Thread)
	{
		// Kill() is a blocking call, it waits for the thread to finish.
		// Hopefully that doesn't take too long
		m_Thread->Kill();
		delete m_Thread;
	}

	if (m_StopEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(m_StopEvent);
	}
}

void FMyWorker::Start()
{
	// Constructs the actual thread object. It will begin execution immediately
// If you've passed in any inputs, set them up before calling this.
	m_Thread = FRunnableThread::Create(this, *FString::Printf(TEXT("FMyWorker %u"), m_Id));

	//FMath::RandInit(m_Id);
	static uint8 Hue = 0;
	FLinearColor tempcolor = FLinearColor::MakeFromHSV8(Hue, 255, 255);
	Hue+=8;
	Hue = Hue % 256;
	m_Color = tempcolor.ToFColor(true);

	//m_Color = FColor::MakeRandomColor();
	//m_Color.A = 255;
}

void FMyWorker::StopNow()
{
	m_StopEvent->Trigger();
}

bool FMyWorker::Init()
{
	UE_LOG(LogTemp, Warning, TEXT("My custom thread has been initialized"));

	m_StopEvent = FPlatformProcess::GetSynchEventFromPool();


	// 	FMath::RandInit(m_Id);
	// // 	FLinearColor tempcolor = FLinearColor::MakeFromHSV8(Rand, 255, 255);
	// // 	m_Color = tempcolor.ToFColor(true);
	// 	m_Color = FColor::MakeRandomColor();
	// 	m_Color.A = 255;

		// Return false if you want to abort the thread
	return true;
}


uint32 FMyWorker::Run()
{
	// Peform your processor intensive task here. In this example, a neverending
	// task is created, which will only end when Stop is called.
	while (!m_StopEvent->Wait(0.f) && m_Ticks < m_NumTicks)
	{
		UE_LOG(LogTemp, Warning, TEXT("My custom thread is running!"))
			FPlatformProcess::Sleep(1.0f);

		if (GEngine)
			GEngine->AddOnScreenDebugMessage(456 + (int32)m_Id, 1.f, m_Color, FString::Printf(TEXT("id:%u tick:%u/%u"), m_Id, m_Ticks, m_NumTicks));

		m_Ticks++;
		m_OnTickDelegate.ExecuteIfBound();
	}

	m_OnDoneDelegate.ExecuteIfBound();

	return 0;
}


// This function is NOT run on the new thread!
void FMyWorker::Stop()
{
	// Clean up memory usage here, and make sure the Run() function stops soon
	// The main thread will be stopped until this finishes!

	// For this example, we just need to terminate the while loop
	// It will finish in <= 1 sec, due to the Sleep()
	m_bStopThread = false;
}

//////////////////////////////////////////////////////////////////////////



PRAGMA_ENABLE_OPTIMIZATION