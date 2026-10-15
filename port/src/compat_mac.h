/* macOS counterparts of the Linux-only thread calls the runtime and the GPU library use.
 * Included by runtime.h and force-included into the GPU library (gpu/CMakeLists.txt), so it
 * is C and C++. Functions rather than macros: none of these names exists in the macOS
 * headers, so nothing in a system header gets renamed. */
#ifndef TLG_COMPAT_MAC_H
#define TLG_COMPAT_MAC_H
#ifdef __APPLE__
#include <stdint.h>
#include <errno.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <signal.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
/* <mach/vm_param.h> macros that would rewrite this project's own functions (probe.c round_page). */
#undef round_page
#undef trunc_page
#ifdef __cplusplus
extern "C" {
#endif

/* Thread id for identity checks and logs: the system-wide id lldb and Activity Monitor show.
 * Not syscall(SYS_gettid): that number exists on macOS but returns the thread's uid/gid. */
static inline pid_t gettid(void) {
    uint64_t id = 0;
    pthread_threadid_np(NULL, &id);
    return (pid_t)id;
}
/* macOS does not tell a thread which core it runs on. A stable per-thread value keeps the
 * game's per-core caches (sceKernelGetCurrentCpu) spread out, as on a host that pins threads. */
static inline int sched_getcpu(void) { return (int)((uint32_t)gettid() % 64u); }
/* Logical CPUs: there are no affinity masks (cpu_set_t) to count on macOS. */
static inline unsigned compat_cpu_count(void) {
    int count = 0;
    size_t size = sizeof(count);
    return sysctlbyname("hw.logicalcpu", &count, &size, NULL, 0) == 0 && count > 0 ? (unsigned)count : 1u;
}
/* The thread's stack as attributes pthread_attr_getstack can read, like glibc's.
 * pthread_get_stackaddr_np returns the TOP (highest address) of the stack, not its base. */
static inline int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) {
    const size_t size = pthread_get_stacksize_np(thread);
    char *const top = (char *)pthread_get_stackaddr_np(thread);
    int error = pthread_attr_init(attr);
    if (!error && (error = pthread_attr_setstack(attr, top - size, size)) != 0) pthread_attr_destroy(attr);
    return error;
}
/* User and system CPU time of a thread (its Mach port): what Linux reads through
 * getrusage(RUSAGE_THREAD) or a pthread_getcpuclockid clock. */
static inline int compat_thread_times(mach_port_t thread, uint64_t *user_us, uint64_t *system_us) {
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)&info, &count) != KERN_SUCCESS) return -1;
    *user_us = (uint64_t)info.user_time.seconds * 1000000u + (uint64_t)info.user_time.microseconds;
    *system_us = (uint64_t)info.system_time.seconds * 1000000u + (uint64_t)info.system_time.microseconds;
    return 0;
}
/* The calling thread's port; pthread_mach_thread_np takes no reference (mach_thread_self does). */
static inline mach_port_t compat_thread_self(void) { return pthread_mach_thread_np(pthread_self()); }

/* Diagnostics (crash dumps, freeze report). No /proc on macOS: threads come from Mach.
 * process_vm_readv for this process only: a read that fails instead of faulting. */
static inline ssize_t process_vm_readv(pid_t pid, const struct iovec *local, unsigned long local_count,
                                       const struct iovec *remote, unsigned long remote_count,
                                       unsigned long flags) {
    (void)flags;
    if (pid != getpid() || local_count != 1 || remote_count != 1 || local->iov_len < remote->iov_len) {
        errno = EINVAL;
        return -1;
    }
    mach_vm_size_t got = 0;
    if (mach_vm_read_overwrite(mach_task_self(), (mach_vm_address_t)(uintptr_t)remote->iov_base,
                               remote->iov_len, (mach_vm_address_t)(uintptr_t)local->iov_base, &got) != KERN_SUCCESS) {
        errno = EFAULT;
        return -1;
    }
    return (ssize_t)got;
}
/* Sends sig to every other thread of the process (Linux: tgkill over /proc/self/task),
 * waiting pause_us after each; threads not created through pthreads are skipped. */
static inline void compat_signal_threads(int sig, unsigned pause_us) {
    thread_act_array_t list;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS) return;
    const mach_port_t self = compat_thread_self();
    for (mach_msg_type_number_t i = 0; i < count; ++i) {
        pthread_t thread = list[i] == self ? NULL : pthread_from_mach_thread_np(list[i]);
        if (thread && pthread_kill(thread, sig) == 0 && pause_us) usleep(pause_us);
        mach_port_deallocate(mach_task_self(), list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)list, count * sizeof(*list));
}
/* gettid() of a Mach thread, as the thread itself would see it. */
static inline uint64_t compat_thread_tid(mach_port_t thread) {
    thread_identifier_info_data_t info;
    mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
    if (thread_info(thread, THREAD_IDENTIFIER_INFO, (thread_info_t)&info, &count) != KERN_SUCCESS) return 0;
    return (uint64_t)(pid_t)info.thread_id;
}

/* struct stat: macOS names the timespec fields st_atimespec, st_mtimespec, st_ctimespec. */
#define st_atim st_atimespec
#define st_mtim st_mtimespec
#define st_ctim st_ctimespec
/* Coarse clocks: macOS reads its clocks from the commpage, cheap enough without them. */
#ifndef CLOCK_REALTIME_COARSE
#define CLOCK_REALTIME_COARSE CLOCK_REALTIME
#endif
#ifndef CLOCK_MONOTONIC_COARSE
#define CLOCK_MONOTONIC_COARSE CLOCK_MONOTONIC
#endif
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#endif
/* getrandom: getentropy gives at most 256 bytes per call. */
static inline ssize_t getrandom(void *buffer, size_t size, unsigned flags) {
    (void)flags;
    for (size_t done = 0; done < size; done += 256)
        if (getentropy((char *)buffer + done, size - done < 256 ? size - done : 256)) return -1;
    return (ssize_t)size;
}

/* Timed locks: macOS has none. Try the lock, then back off (yield, then sleeps of 50 us
 * doubling to 1 ms) until the absolute CLOCK_REALTIME deadline. Only the guest's timed calls
 * come here; untimed locks block natively. Costs up to ~1 ms of latency under contention,
 * and a polling writer does not queue ahead of new readers. */
static inline int compat_backoff(const struct timespec *deadline, unsigned *round) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (now.tv_sec > deadline->tv_sec || (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec))
        return ETIMEDOUT;
    if (*round < 16) { ++*round; sched_yield(); return 0; }
    const int64_t left = (int64_t)(deadline->tv_sec - now.tv_sec) * 1000000000 + (deadline->tv_nsec - now.tv_nsec);
    const unsigned shift = *round - 16 < 5 ? *round - 16 : 5;
    int64_t pause = INT64_C(50000) << shift;
    if (pause > 1000000) pause = 1000000;
    if (pause > left) pause = left;
    ++*round;
    const struct timespec t = {0, (long)pause};
    nanosleep(&t, NULL);
    return 0;
}
static inline int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *deadline) {
    unsigned round = 0;
    int e;
    while ((e = pthread_mutex_trylock(mutex)) == EBUSY)
        if (compat_backoff(deadline, &round)) return ETIMEDOUT;
    return e;
}
static inline int pthread_rwlock_timedrdlock(pthread_rwlock_t *lock, const struct timespec *deadline) {
    unsigned round = 0;
    int e;
    while ((e = pthread_rwlock_tryrdlock(lock)) == EBUSY)
        if (compat_backoff(deadline, &round)) return ETIMEDOUT;
    return e;
}
static inline int pthread_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *deadline) {
    unsigned round = 0;
    int e;
    while ((e = pthread_rwlock_trywrlock(lock)) == EBUSY)
        if (compat_backoff(deadline, &round)) return ETIMEDOUT;
    return e;
}

#ifdef __cplusplus
}
#endif
#endif
#endif
