#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Fixed-size thread pool with a bounded task queue.
//
// Workers block on a condition variable waiting for tasks. submit() enqueues
// a task and wakes one worker. If the queue is full, submit() returns false
// so the caller can reject the request rather than block or allocate unboundedly.
// The destructor signals all workers to finish their current task and exit.
class ThreadPool {
public:
    // Spawns `threads` worker threads; rejects submissions once `max_queue`
    // tasks are already waiting.
    explicit ThreadPool(size_t threads, size_t max_queue = 64)
        : max_queue_(max_queue) {
        for (size_t i = 0; i < threads; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }

    // Signals stop, wakes all workers, joins them. In-flight tasks finish first.
    ~ThreadPool() {
        { std::lock_guard<std::mutex> lock(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    // Enqueues a task. Returns false if queue is full or pool is shutting down.
    bool submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_ || queue_.size() >= max_queue_) return false;
            queue_.push(std::move(task));
        }
        cv_.notify_one();  // wake exactly one idle worker
        return true;
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                // sleep until there is work to do or we are asked to stop
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop();
            }
            try { task(); } catch (...) {}  // never let a task kill the worker
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> queue_;
    std::mutex                        mtx_;
    std::condition_variable           cv_;
    bool                              stop_      = false;
    size_t                            max_queue_;
};
