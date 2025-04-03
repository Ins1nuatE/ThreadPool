#include <unistd.h>

#include <future>
#include <iostream>
#include <vector>

#include "task_queue.h"
#include "thread_pool.h"

const std::vector<int> kNums = {1, 5, 2, 4, 3};

void ThreadPoolTest() {
  std::cout << "Thread pool test started." << std::endl;
  ThreadPool thread_pool;
  for (int num : kNums) {
    thread_pool.Submit([num] {
      sleep(num);
      std::cout << num << std::endl;
    });
  }
}

void TaskQueueTest() {
  std::cout << "Task queue test started." << std::endl;
  ThreadPool thread_pool;
  TaskQueue task_queue(&thread_pool);
  for (int num : kNums) {
    task_queue.Enqueue([num] {
      sleep(num);
      std::cout << num << std::endl;
    });
  }
}

int Add(int a, int b) { return a + b; }

void ReturnValueTest() {
  std::cout << "Return value test started." << std::endl;
  ThreadPool thread_pool;
  TaskQueue task_queue(&thread_pool);
  int a = 1, b = 2;
  auto result = task_queue.Enqueue(Add, a, b);
  std::cout << a << " + " << b << " = " << result.get() << std::endl;
}

int main() {
  ThreadPoolTest();
  TaskQueueTest();
  ReturnValueTest();

  return 0;
}
