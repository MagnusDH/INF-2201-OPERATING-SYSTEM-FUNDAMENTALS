#include "thread.h"

void spinlock_init(volatile int *s) {
}

void spinlock_acquire(volatile int *s) {
}

void spinlock_release(volatile int *s) {
}

void lock_init(lock_t *l) {
}

void lock_acquire(lock_t *l) {
}

void lock_release(lock_t *l) {
}

void condition_init(condition_t *c) {
}

void condition_wait(lock_t *m, condition_t *c) {
}

void condition_signal(condition_t *c) {
}

void condition_broadcast(condition_t *c) {
}
