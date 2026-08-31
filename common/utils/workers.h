#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <new>

class Workers final {
  public:
    struct Telemetry final {
      uint32_t hardwareConcurrency  = 0;
      uint8_t  participantLimit     = 0;
      uint32_t lastWorkersRequested = 0;
      uint8_t  lastWorkersUsed      = 0;
      uint8_t  maxWorkersUsed       = 0;
      uint64_t dispatches           = 0;
      uint64_t workerWakeups        = 0;
      uint64_t workersUsedTotal     = 0;
      uint64_t mainWaitYields       = 0;
      uint64_t mainWaitNs           = 0;
      };

    Workers();
    ~Workers();

    static void setThreadName(const char* threadName);

    // Zero preserves the historic automatic worker count. Explicit values are
    // total parallel participants, including the calling/main thread.
    static bool      setParticipantLimit(uint8_t participants);
    static uint8_t   participantLimit();
    // Hot-path counters are collected only in OPENGOTHIC_PERF_DIAGNOSTICS
    // builds; hardwareConcurrency and participantLimit are always available.
    static Telemetry telemetrySnapshot();
    static void      resetTelemetry();

    template<class T,class F>
    static void parallelFor(T* b, T* e, const F& func) {
      inst().runParallelFor(b,std::distance(b,e),func);
      }

    template<class T,class F>
    static void parallelFor(std::vector<T>& data, const F& func) {
      inst().runParallelFor(data.data(),data.size(),func);
      }

    template<class T,class F>
    static void parallelTasks(std::vector<T>& data, const F& func) {
      inst().runParallelFor(data.data(),data.size(),func);
      }

    template<class F>
    static void parallelTasks(size_t taskCount, const F& func) {
      inst().runParallelTasks<F>(taskCount,func);
      }

    static uint8_t maxThreads();

  private:
    enum { MAX_THREADS=16 };

    void            threadFunc(size_t id);
    void            execWork(uint32_t& minWorkSize);
    uint32_t        taskLoop();
    uint32_t        indexedTaskLoop();
    static Workers& inst();

    template<class T,class F>
    static uint32_t&  minWorkSize() {
      static uint32_t data = 0;
      return data;
      }


    template<class T,class F>
    void runParallelFor(T* data, size_t sz, const F& func) {
      workSet     = reinterpret_cast<uint8_t*>(data);
      workSize    = sz;
      workEltSize = sizeof(T);

      workFunc = [&func](void* data, size_t sz) {
        T* tdata = reinterpret_cast<T*>(data);
        for(size_t i=0;i<sz;++i)
          func(tdata[i]);
        };

      execWork(minWorkSize<T,F>());
      }

    template<class F>
    void runParallelTasks(size_t taskCount, const F& func) {
      workSet     = nullptr;
      workSize    = taskCount;
      workEltSize = 1;

      workFunc = [&func](void* data, size_t sz) {
        func(reinterpret_cast<uintptr_t>(data));
        };
      execWork(minWorkSize<void,F>());
      }

    static const size_t               taskPerThread;
    static const size_t               taskPerStep;
    bool                              running=true;

    std::thread                       th[MAX_THREADS];

    std::mutex                        sync;
    std::condition_variable           workWait;
    int32_t                           workTbd = 0;

    uint8_t*                          workSet=nullptr;
    size_t                            workSize=0, workEltSize=0;
    std::function<void(void*,size_t)> workFunc;

    std::atomic_int                   progressIt{0};
    uint32_t                          taskCount = 0;
    std::atomic_int                   taskDone{0};
    bool                              indexedTaskMode = false;

    std::atomic_uint8_t               participantLimitCfg{0};

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    std::atomic_uint32_t              telemetryWorkerMask{0};
    std::atomic_uint32_t              telemetryLastWorkersRequested{0};
    std::atomic_uint8_t               telemetryLastWorkersUsed{0};
    std::atomic_uint8_t               telemetryMaxWorkersUsed{0};
    std::atomic_uint64_t              telemetryDispatches{0};
    std::atomic_uint64_t              telemetryWorkerWakeups{0};
    std::atomic_uint64_t              telemetryWorkersUsedTotal{0};
    std::atomic_uint64_t              telemetryMainWaitYields{0};
    std::atomic_uint64_t              telemetryMainWaitNs{0};
#endif
  };
