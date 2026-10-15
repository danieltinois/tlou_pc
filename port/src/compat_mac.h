/* macOS counterparts of the Linux-only thread calls the runtime and the GPU library use.
 * Included by runtime.h and force-included into the GPU library (gpu/CMakeLists.txt), so it
 * is C and C++. Functions rather than macros: none of these names exists in the macOS
 * headers, so nothing in a system header gets renamed. */
#ifndef TLG_COMPAT_MAC_H
#define TLG_COMPAT_MAC_H
#ifdef __APPLE__
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
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

#ifdef __cplusplus
}
#endif
#endif
#endif
