// schedulertest.c -- workload for comparing RR, FIFO and MLFQ.
//
// Spawns NFORK children with deliberately different CPU/IO behaviour:
//
//   child 0, 1 : pure CPU bound, never give up the CPU voluntarily
//   child 2, 3 : IO bound, short burst of work then pause() (sleeps)
//   child 4    : long CPU bound job
//
// The parent reaps each child with waitx() and prints per-child and average
// turnaround / waiting / response times, measured in timer ticks.
//
// Build the kernel with the policy you want to measure, then run this:
//
//   make clean; make qemu CPUS=1                    (round robin)
//   make clean; make qemu CPUS=1 SCHEDULER=FIFO     (first come first served)
//   make clean; make qemu CPUS=1 SCHEDULER=MLFQ     (multi level feedback)
//
//   $ schedulertest

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NFORK 5

// Tune these if the test finishes too fast or too slowly under qemu.
#define BURST_SHORT 12000000L   // one short slice of busy work
#define BURST_LONG  150000000L   // one long slice of busy work
#define IO_ROUNDS   15          // how many work/sleep cycles an IO child does
#define CPU_ROUNDS  4          // how many long bursts a CPU child does

// Busy work the compiler cannot optimise away.
static void
burn(long iterations)
{
  volatile long x = 0;
  for (long i = 0; i < iterations; i++)
    x = x + i;
}

int
main(void)
{
  int n = 0;

  printf("schedulertest: spawning %d children\n", NFORK);

  for (int i = 0; i < NFORK; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("schedulertest: fork failed\n");
      break;
    }
    if (pid == 0) {
      // ---- child ----
      if (i < 2) {
        // CPU bound: pure computation, gets demoted down the MLFQ queues
        for (int r = 0; r < CPU_ROUNDS; r++)
          burn(BURST_LONG);
      } else if (i < 4) {
        // IO bound: a little work, then block. Should stay near queue 0.
        for (int r = 0; r < IO_ROUNDS; r++) {
          burn(BURST_SHORT);
          pause(2);            // xv6's sleep(): blocks for 2 ticks
        }
      } else {
        // one long CPU hog, to make FIFO's starvation obvious
        for (int r = 0; r < CPU_ROUNDS * 2; r++)
          burn(BURST_LONG);
      }
      exit(0);
    }
    n++;
  }

  int total_turn = 0, total_wait = 0, total_resp = 0, counted = 0;

  printf("\n pid   turnaround   waiting   response   running\n");
  printf("-------------------------------------------------\n");

  for (int i = 0; i < n; i++) {
    int status, rtime, wtime, restime;
    int pid = waitx(&status, &rtime, &wtime, &restime);
    if (pid < 0) {
      printf("schedulertest: waitx failed\n");
      break;
    }
    int turn = rtime + wtime;
    printf("%d %d %d %d %d\n", pid, turn, wtime, restime, rtime);

    total_turn += turn;
    total_wait += wtime;
    total_resp += restime;
    counted++;
  }

  if (counted > 0) {
    printf("-------------------------------------------------\n");
    printf("children: %d\n", counted);
    printf("avg turnaround: %d ticks\n", total_turn / counted);
    printf("avg waiting:    %d ticks\n", total_wait / counted);
    printf("avg response:   %d ticks\n", total_resp / counted);
  }

  exit(0);
}