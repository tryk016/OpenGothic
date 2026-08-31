#include "workers.h"
#include "utils/string_frm.h"

#include <Tempest/Platform>
#include <Tempest/Log>

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
#include <chrono>
#endif

#if defined(__WINDOWS__)
#include <windows.h>
#include <processthreadsapi.h>
#endif

#if defined(__GNUC__)
#include <pthread.h>
#endif

#if defined(_MSC_VER)
void Workers::setThreadName(const char* threadName) {
  const DWORD MS_VC_EXCEPTION = 0x406D1388;
  DWORD dwThreadID = GetCurrentThreadId();
#pragma pack(push,8)
  struct THREADNAME_INFO {
    DWORD  dwType = 0x1000; // Must be 0x1000.
    LPCSTR szName;          // Pointer to name (in user addr space).
    DWORD  dwThreadID;      // Thread ID (-1=caller thread).
    DWORD  dwFlags;         // Reserved for future use, must be zero.
    };
#pragma pack(pop)

  THREADNAME_INFO info = {};
  info.szName     = threadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags    = 0;

  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
    }
  __except(EXCEPTION_EXECUTE_HANDLER) {
    }
  }
#elif defined(__WINDOWS__)
void Workers::setThreadName(const char* threadName) {
#if defined(__GNUC__)
  pthread_setname_np(pthread_self(), threadName);
#endif
  auto k32 = GetModuleHandleA("Kernel32");
  auto fn  = GetProcAddress(k32, "SetThreadDescription");
  if(fn==nullptr)
    return;

  // nsight does not care about pthread_setname_np
  WCHAR wname[64] = {};
  for(size_t i=0; i<63 && threadName[i]; ++i)
    wname[i] = WCHAR(threadName[i]);
  auto SetThreadDescription = reinterpret_cast<HRESULT(WINAPI*)(HANDLE,PCWSTR)>(fn);
  SetThreadDescription(GetCurrentThread(), wname);
  }
#elif defined(__GNUC__) && !defined(__clang__)
void Workers::setThreadName(const char* threadName){
  pthread_setname_np(pthread_self(), threadName);
  }
#elif defined(__OSX__)
void Workers::setThreadName(const char* threadName){
  pthread_setname_np(threadName);
  }
#else
void Workers::setThreadName(const char* threadName) { (void)threadName; }
#endif

using namespace Tempest;

const size_t Workers::taskPerThread = 128;
const size_t Workers::taskPerStep   = 16;

Workers::Workers() {
  size_t id=0;
  for(auto& i:th) {
    i = std::thread([this,id]() noexcept {
      threadFunc(id);
      });
    ++id;
    }
  }

Workers::~Workers() {
  running  = false;
  workSet  = nullptr;
  workSize = MAX_THREADS;
  execWork(minWorkSize<void,void>());
  for(auto& i:th)
    i.join();
  }

Workers &Workers::inst() {
  static Workers w;
  return w;
  }

uint8_t Workers::maxThreads() {
  int32_t th = int32_t(std::thread::hardware_concurrency());
  if(th<=0)
    th = 1;
  if(th>MAX_THREADS)
    return MAX_THREADS;
  return uint8_t(th);
  }

bool Workers::setParticipantLimit(uint8_t participants) {
  switch(participants) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
      inst().participantLimitCfg.store(participants);
      return true;
    default:
      return false;
    }
  }

uint8_t Workers::participantLimit() {
  return inst().participantLimitCfg.load();
  }

Workers::Telemetry Workers::telemetrySnapshot() {
  auto& w = inst();
  Telemetry ret;
  ret.hardwareConcurrency = std::thread::hardware_concurrency();
  ret.participantLimit    = w.participantLimitCfg.load();
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  ret.lastWorkersRequested = w.telemetryLastWorkersRequested.load();
  ret.lastWorkersUsed      = w.telemetryLastWorkersUsed.load();
  ret.maxWorkersUsed       = w.telemetryMaxWorkersUsed.load();
  ret.dispatches           = w.telemetryDispatches.load();
  ret.workerWakeups        = w.telemetryWorkerWakeups.load();
  ret.workersUsedTotal     = w.telemetryWorkersUsedTotal.load();
  ret.mainWaitYields       = w.telemetryMainWaitYields.load();
  ret.mainWaitNs           = w.telemetryMainWaitNs.load();
#endif
  return ret;
  }

void Workers::resetTelemetry() {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  auto& w = inst();
  w.telemetryWorkerMask.store(0);
  w.telemetryLastWorkersRequested.store(0);
  w.telemetryLastWorkersUsed.store(0);
  w.telemetryMaxWorkersUsed.store(0);
  w.telemetryDispatches.store(0);
  w.telemetryWorkerWakeups.store(0);
  w.telemetryWorkersUsedTotal.store(0);
  w.telemetryMainWaitYields.store(0);
  w.telemetryMainWaitNs.store(0);
#endif
  }

void Workers::threadFunc(size_t id) {
  {
  string_frm tname("Workers [",int(id),"]");
  setThreadName(tname.c_str());
  }

  while(true) {
    {
    std::unique_lock<std::mutex> lck(sync);
    workWait.wait(lck, [this]() { return workTbd>0; });
    --workTbd;
    }

    if(!running) {
      taskDone.fetch_add(1);
      return;
      }

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    telemetryWorkerWakeups.fetch_add(1);
#endif

    bool didWork = false;
    if(workSet==nullptr) {
      if(indexedTaskMode) {
        didWork = indexedTaskLoop()>0;
        } else {
        auto idx = progressIt.fetch_add(1);
        workFunc(reinterpret_cast<void*>(uintptr_t(idx)), 1);
        didWork = true;
        }
      } else {
      didWork = taskLoop()>0;
      }

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    if(didWork)
      telemetryWorkerMask.fetch_or(uint32_t(1u)<<uint32_t(id));
#else
    (void)didWork;
#endif

    taskDone.fetch_add(1);
    // if(size_t(taskDone.fetch_add(1)+1)==taskCount)
    //   std::this_thread::yield();
    }
  }

uint32_t Workers::taskLoop() {
  uint32_t count = 0;
  while(true) {
    size_t b = size_t(progressIt.fetch_add(taskPerStep));
    size_t e = std::min(b+taskPerStep, workSize);
    if(e<=b)
      break;

    void* d = workSet + b*workEltSize;
    workFunc(d,e-b);
    count += uint32_t(e-b);
    }
  return count;
  }

uint32_t Workers::indexedTaskLoop() {
  uint32_t count = 0;
  while(true) {
    const size_t id = size_t(progressIt.fetch_add(1));
    if(id>=workSize)
      break;
    workFunc(reinterpret_cast<void*>(uintptr_t(id)),1);
    ++count;
    }
  return count;
  }

void Workers::execWork(uint32_t& minElts) {
  if(workSize==0)
    return;

  indexedTaskMode = false;
  const uint8_t configuredParticipants = participantLimitCfg.load();
  uint32_t naturalWorkerCount = 0;

  if(workSet!=nullptr) {
    naturalWorkerCount = uint32_t((workSize+taskPerThread-1)/taskPerThread);
    naturalWorkerCount--; // main thread also does tasks

    if(configuredParticipants==0) {
      taskCount = std::min<uint32_t>(naturalWorkerCount,maxThreads());
      // Preserve the historic automatic path: a single calculated worker means
      // the complete operation stays on the calling thread.
      if(taskCount<=1) {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
        telemetryLastWorkersRequested.store(0);
        telemetryLastWorkersUsed.store(0);
#endif
        workFunc(workSet,workSize);
        return;
        }
      } else {
      const uint32_t maxWorkers = uint32_t(configuredParticipants-1u);
      taskCount = std::min(naturalWorkerCount,maxWorkers);
      if(naturalWorkerCount<=1 || taskCount==0) {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
        telemetryLastWorkersRequested.store(0);
        telemetryLastWorkersUsed.store(0);
#endif
        workFunc(workSet,workSize);
        return;
        }
      }
    } else {
    if(!running) {
      // Destruction must wake every persistent worker, regardless of a QA cap.
      taskCount = uint32_t(workSize);
      } else if(configuredParticipants==0) {
      taskCount = uint32_t(workSize);
      if(taskCount==1) {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
        telemetryLastWorkersRequested.store(0);
        telemetryLastWorkersUsed.store(0);
#endif
        workFunc(workSet,workSize);
        return;
        }
      } else {
      if(workSize<=1 || configuredParticipants==1) {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
        telemetryLastWorkersRequested.store(0);
        telemetryLastWorkersUsed.store(0);
#endif
        for(size_t i=0;i<workSize;++i)
          workFunc(reinterpret_cast<void*>(uintptr_t(i)),1);
        return;
        }
      taskCount = std::min<uint32_t>(uint32_t(workSize),uint32_t(configuredParticipants-1u));
      indexedTaskMode = true;
      }
    }

  minElts = std::max<uint32_t>(minElts, taskPerThread);

  if(workSet!=nullptr && workSize<=minElts && true) {
    workFunc(workSet, workSize);
    if(minElts > workSize*2)
      minElts = 0;
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    telemetryLastWorkersRequested.store(0);
    telemetryLastWorkersUsed.store(0);
#endif
    return;
    }

  progressIt.store(0);
  taskDone.store(0);

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  telemetryWorkerMask.store(0);
  telemetryLastWorkersRequested.store(taskCount);
  telemetryDispatches.fetch_add(1);
#endif

  {
  std::unique_lock<std::mutex> lck(sync);
  workTbd = int32_t(taskCount);
  }
  workWait.notify_all();

  uint32_t cnt = 0;
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  uint64_t yieldCount = 0;
#endif
  if(workSet==nullptr) {
    if(indexedTaskMode) {
      cnt = indexedTaskLoop(); (void)cnt;
      }
    } else {
    cnt = taskLoop(); (void)cnt;
    }

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  const auto waitBegin = std::chrono::steady_clock::now();
#endif
  if(workSet==nullptr && !indexedTaskMode) {
    std::this_thread::yield();
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    ++yieldCount;
#endif
    }
  while(true) {
    int expect = int(taskCount);
    if(taskDone.load()==expect) {
      taskDone.store(0);
      break;
      }
    std::this_thread::yield();
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    ++yieldCount;
#endif
    }
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  const auto waitEnd = std::chrono::steady_clock::now();
  const uint64_t waitNs = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(waitEnd-waitBegin).count());
  uint32_t mask = telemetryWorkerMask.load();
  uint8_t workersUsed = 0;
  while(mask!=0) {
    workersUsed += uint8_t(mask&1u);
    mask >>= 1u;
    }
  telemetryLastWorkersUsed.store(workersUsed);
  telemetryWorkersUsedTotal.fetch_add(workersUsed);
  telemetryMainWaitYields.fetch_add(yieldCount);
  telemetryMainWaitNs.fetch_add(waitNs);

  uint8_t maxUsed = telemetryMaxWorkersUsed.load();
  while(maxUsed<workersUsed &&
        !telemetryMaxWorkersUsed.compare_exchange_weak(maxUsed,workersUsed)) {
    }
#endif
  }
