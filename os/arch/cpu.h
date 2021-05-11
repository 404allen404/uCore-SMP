#if !defined(CPU_H)
#define CPU_H

#include <ucore/types.h>

#if !defined(NCPU)
#define NCPU 1
#endif // NCPU

extern volatile int booted[NCPU];
extern volatile int halted[NCPU];

// Saved registers for kernel context switches.
struct context
{
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu
{
  struct proc *proc;      // The process running on this cpu, or null.
  struct context context; // swtch() here to enter scheduler().
  int noff;               // Depth of push_off() nesting.
  int base_interrupt_status;        // Were interrupts enabled before push_off()?
  int core_id;
};

// debug print
void print_cpu(struct cpu *c);
extern struct cpu cpus[NCPU];

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *mycpu(void);

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid();

void halt();

void wait_all_halt();

void init_cpu();

#endif // CPU_H
