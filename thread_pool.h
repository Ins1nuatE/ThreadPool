#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
 public:
  using TaskFunc = std::function<void()>;

  ThreadPool(unsigned int thread_count = std::thread::hardware_concurrency());
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  template <typename F, typename... Args>
  auto Submit(F &&f, Args &&...arg_list)
      -> std::future<std::invoke_result_t<F, Args...>>;

  void Wait();

 private:
  std::atomic<bool> shutdown_ = {false};

  unsigned int thread_count_;
  std::vector<std::thread> threads_;

  std::mutex active_mutex_;
  std::condition_variable active_cv_;
  unsigned int active_threads_;

  std::mutex task_mutex_;
  std::condition_variable task_cv_;
  std::queue<TaskFunc> task_queue_;
};

ThreadPool::ThreadPool(unsigned int thread_count)
    : thread_count_(thread_count), active_threads_(0) {
  for (unsigned int i = 0; i < thread_count_; ++i) {
    threads_.emplace_back([&] {
      while (true) {
        TaskFunc task;
        {
          std::unique_lock<std::mutex> task_lock(task_mutex_);
          task_cv_.wait(task_lock,
                        [&] { return shutdown_ || !task_queue_.empty(); });
          if (shutdown_ && task_queue_.empty()) {
            return;
          }
          {
            std::unique_lock<std::mutex> active_lock(active_mutex_);
            ++active_threads_;
          }
          task = std::move(task_queue_.front());
          task_queue_.pop();
        }
        if (task) {
          task();
        }
        {
          std::unique_lock<std::mutex> active_lock(active_mutex_);
          --active_threads_;
        }
        active_cv_.notify_all();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  shutdown_ = true;
  task_cv_.notify_all();
  for (unsigned int i = 0; i < threads_.size(); ++i) {
    if (threads_[i].joinable()) {
      threads_[i].join();
    }
  }
}

template <typename F, typename... Args>
auto ThreadPool::Submit(F &&f, Args &&...arg_list)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using ReturnType = std::invoke_result_t<F, Args...>;

  std::function<ReturnType()> func =
      std::bind(std::forward<F>(f), std::forward<Args>(arg_list)...);
  auto task_ptr = std::make_shared<std::packaged_task<ReturnType()>>(func);
  TaskFunc warpper_func = [task_ptr] { (*task_ptr)(); };
  {
    std::unique_lock<std::mutex> lock(task_mutex_);
    task_queue_.push(std::move(warpper_func));
  }
  task_cv_.notify_one();

  return task_ptr->get_future();
}

void ThreadPool::Wait() {
  std::unique_lock<std::mutex> lock(active_mutex_);
  active_cv_.wait(lock,
                  [&] { return active_threads_ == 0 && task_queue_.empty(); });
}

#endif  // THREAD_POOL_H_
