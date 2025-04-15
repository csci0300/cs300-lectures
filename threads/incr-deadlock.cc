#include <cstdio>
#include <mutex>
#include <thread>

#include "helpers.hh"

std::mutex m1;
int v1 = 0;

std::mutex m2;
int v2 = 0;

void t1() {
  int i = 0;
  while (1) {
    m1.lock();
    m2.lock();

    v1 = 42;
    v2 = 1;

    m1.unlock();
    m2.unlock();

    if ((i % 100) == 0) {
      printf("t1: attempt %d\n", i);
    }
    i++;
  }
}

void t2() {
  int i = 0;
  while (1) {
    m2.lock();
    m1.lock();

    v1 = 128;
    v2 = 5;

    m2.unlock();
    m1.unlock();

    if ((i % 100) == 0) {
      printf("t2: attempt %d\n", i);
    }
    i++;

  }
}

int main() {
  std::thread th1(t1);
  std::thread th2(t2);

  th1.join();
  th2.join();
}
