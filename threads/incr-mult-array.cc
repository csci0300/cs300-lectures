#include <cstdio>
#include <thread>
#include <mutex>
#include <math.h>
#include "helpers.hh"

#define NUM_THREADS 1
#define ARRAY_SIZE 100000

void setup();

struct item {
    std::mutex mtx;
    int num;
};

struct item all_items[ARRAY_SIZE];

void threadfunc(int tid) {
    unsigned int seed = tid;
    
    for (int i = 0; i != 10000000; ++i) {
	int idx1 = rand_r(&seed) % ARRAY_SIZE; // Get random int [0, ARRAY_SIZE)
	int idx2 = rand_r(&seed) % ARRAY_SIZE; // Get random int [0, ARRAY_SIZE)
	all_items[idx1].mtx.lock();
	all_items[idx2].mtx.lock();

	all_items[idx1].num *= all_items[idx2].num;

	all_items[idx1].mtx.unlock();
	all_items[idx2].mtx.unlock();

	if ((i % 100000) == 0) {
	    printf("Thread %d:  %d iterations\n", tid, i);
	}
    }
}

int main() {
    std::thread th[NUM_THREADS];
    setup();

    for (int i = 0; i != NUM_THREADS; ++i) {
        th[i] = std::thread(threadfunc, i);
    }
    for (int i = 0; i != NUM_THREADS; ++i) {
        th[i].join();
    }
}


void setup() {
    for (int i = 0; i < ARRAY_SIZE; i++) {
	all_items[i].num = 0;
    }
}
