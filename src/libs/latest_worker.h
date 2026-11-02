#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

// One active job and one replaceable pending job. Callers own cancellation
// flags, so abandoning a request never joins a decoder on the UI thread.
class LatestWorker {
    std::mutex mutex;
    std::condition_variable cv;
    std::function<void()> pending;
    bool stopping = false;
    std::thread worker;

public:
    LatestWorker()
        : worker([this] {
              std::unique_lock lock(mutex);
              for (;;) {
                  cv.wait(lock, [this] { return stopping || static_cast<bool>(pending); });
                  if (stopping) {
                      return;
                  }
                  auto job = std::move(pending);
                  pending = {};
                  lock.unlock();
                  job();
                  job = {}; // Release retired PCM outside the submission lock.
                  lock.lock();
              }
          }) {}
    ~LatestWorker() {
        {
            std::scoped_lock lock(mutex);
            stopping = true;
            pending = {};
        }
        cv.notify_one();
        worker.join();
    }
    void submit(std::function<void()> job) {
        {
            std::scoped_lock lock(mutex);
            pending = std::move(job);
        }
        cv.notify_one();
    }
};
