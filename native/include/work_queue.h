// work_queue.h — single background thread that serialises one-shot D-Bus
// method calls for TechnologyBridge and ServiceBridge.
//
// Replaces the original per-call std::thread::detach() pattern which created
// an unbounded number of threads and D-Bus connections.  One WorkQueue +
// one shared sdbus::IConnection is shared by all method dispatches.

#pragma once

#include <pthread.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class WorkQueue {
 public:
  WorkQueue() : running_(true) {
    thread_ = std::thread([this] { run(); });
  }

  // Joins the worker thread — all enqueued tasks complete before returning.
  ~WorkQueue() noexcept {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      running_ = false;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  WorkQueue(const WorkQueue&) = delete;
  WorkQueue& operator=(const WorkQueue&) = delete;

  /// Enqueue a task for execution on the worker thread.
  /// Thread-safe; may be called from any thread.
  void enqueue(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(std::move(task));
    }
    cv_.notify_one();
  }

 private:
  void run() {
    pthread_setname_np(pthread_self(), "connman_work");
    while (true) {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
      while (!queue_.empty()) {
        auto task = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        task();  // execute without holding the mutex
        lock.lock();
      }
      if (!running_) {
        break;
      }
    }
  }

  std::atomic<bool> running_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> queue_;
  std::thread thread_;
};
