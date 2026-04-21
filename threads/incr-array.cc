#include <cstdio>
#include <thread>
#include <mutex>
#include <math.h>
#include "helpers.hh"

#define NUM_THREADS 4
#define ARRAY_SIZE 100000

void setup();

struct item {
    int count;  
    int last_update_by;
};

struct item all_items[ARRAY_SIZE];

void threadfunc(int tid) {
    unsigned int seed = tid;
    
    for (int i = 0; i != 10000000; ++i) {
	int index = rand_r(&seed) % ARRAY_SIZE; // Get random int [0, ARRAY_SIZE)

	all_items[index].count += 1;
	all_items[index].last_update_by = tid;
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

    unsigned long total = 0;
    for(int i = 0; i < ARRAY_SIZE; ++i) {
	total += all_items[i].count;
    }
    printf("Total:  %ld\n", total);
}


void setup() {
    for (int i = 0; i < ARRAY_SIZE; i++) {
	all_items[i].count = 0;
	all_items[i].last_update_by = -1;
    }
}
