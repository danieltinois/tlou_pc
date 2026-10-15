/* General registers saved in a signal's ucontext, as an array indexed by REG_* on both hosts:
 *   Linux: uc_mcontext.gregs[REG_RIP] (glibc's REG_* indices, _GNU_SOURCE).
 *   macOS: uc_mcontext->__ss, an x86_thread_state64 struct whose first fields are rax, rbx,
 *          rcx, rdx, rdi, rsi, rbp, rsp, r8-r15, rip, rflags; REG_* here are those positions.
 * Under Rosetta 2 the signal frame is the same x86_64 layout. Not for Windows (CONTEXT). */
#ifndef TLG_UCONTEXT_REGS_H
#define TLG_UCONTEXT_REGS_H
#ifdef __APPLE__
#include <stdint.h>
#include <stddef.h>
#include <sys/ucontext.h>
typedef uint64_t greg_t;
enum {
    REG_RAX, REG_RBX, REG_RCX, REG_RDX, REG_RDI, REG_RSI, REG_RBP, REG_RSP,
    REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15, REG_RIP, REG_EFL
};
#define UC_GREGS(uc) ((greg_t *)&(uc)->uc_mcontext->__ss)
#ifdef __cplusplus
static_assert(offsetof(_STRUCT_X86_THREAD_STATE64, __rip) == REG_RIP * 8, "x86_thread_state64 layout");
static_assert(offsetof(_STRUCT_X86_THREAD_STATE64, __rflags) == REG_EFL * 8, "x86_thread_state64 layout");
#else
_Static_assert(offsetof(_STRUCT_X86_THREAD_STATE64, __rip) == REG_RIP * 8, "x86_thread_state64 layout");
_Static_assert(offsetof(_STRUCT_X86_THREAD_STATE64, __rflags) == REG_EFL * 8, "x86_thread_state64 layout");
#endif
#else
#include <ucontext.h>
#define UC_GREGS(uc) ((uc)->uc_mcontext.gregs)
#endif
#endif
