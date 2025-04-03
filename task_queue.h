#ifndef TASK_QUEUE_H_
#define TASK_QUEUE_H_

#include "thread_pool.h"

class TaskQueue {
  using TaskFunc = std::function<void()>;

  template <typename F, typename... Args>
  struct Task {
    using ReturnType = std::invoke_result_t<F, Args...>;

    explicit Task(TaskQueue *parent, F &&f, Args &&...arg_list)
        : parent_(parent) {
      std::function<ReturnType()> func =
          std::bind(std::forward<F>(f), std::forward<Args>(arg_list)...);
      task_ptr_ = std::make_shared<std::packaged_task<ReturnType()>>(func);
    }

    void operator()() noexcept {
      (*task_ptr_)();
      parent_->TaskCompleted();
    }

    TaskQueue *parent_;
    std::shared_ptr<std::packaged_task<ReturnType()>> task_ptr_;
  };

 public:
  TaskQueue(ThreadPool *thread_pool);
  ~TaskQueue();

  TaskQueue(const TaskQueue &) = delete;
  TaskQueue &operator=(const TaskQueue &) = delete;

  TaskQueue(TaskQueue &&) = default;
  TaskQueue &operator=(TaskQueue &&) = default;

  template <typename F, typename... Args>
  auto Enqueue(F &&f, Args &&...arg_list)
      -> std::future<std::invoke_result_t<F, Args...>>;

 private:
  void TaskCompleted();

  ThreadPool *thread_pool_;
  bool task_running_ = false;
  std::mutex task_mutex_;
  std::queue<TaskFunc> task_queue_;
};

TaskQueue::TaskQueue(ThreadPool *thread_pool) : thread_pool_(thread_pool) {}

TaskQueue::~TaskQueue() { thread_pool_->Wait(); }

template <typename F, typename... Args>
auto TaskQueue::Enqueue(F &&f, Args &&...arg_list)
    -> std::future<std::invoke_result_t<F, Args...>> {
  Task task(this, std::move(f), std::move(arg_list)...);
  auto future = task.task_ptr_->get_future();
  {
    std::unique_lock<std::mutex> lock(task_mutex_);
    if (task_running_) {
      task_queue_.push(std::move(task));
    } else {
      thread_pool_->Submit(std::move(task));
      task_running_ = true;
    }
  }
  return std::move(future);
}

void TaskQueue::TaskCompleted() {
  TaskFunc next_task;
  {
    std::unique_lock<std::mutex> lock(task_mutex_);
    if (task_queue_.empty()) {
      task_running_ = false;
      return;
    }
    next_task = std::move(task_queue_.front());
    task_queue_.pop();
  }
  thread_pool_->Submit(std::move(next_task));
}

#endif  // TASK_QUEUE_H_
