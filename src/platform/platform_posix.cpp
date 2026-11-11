#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cpptrace/cpptrace.hpp>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <unwind.h>
#include <vector>

#include "platform.h"

namespace {

    constexpr std::size_t MAX_FRAMES = 64;
    std::atomic_flag g_crashing = ATOMIC_FLAG_INIT;

    void write_all(int fd, const char* buf, std::size_t len) {
        while (len > 0) {
            ssize_t n = write(fd, buf, len);
            if (n > 0) {
                buf += n;
                len -= static_cast<std::size_t>(n);
                continue;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
    }

    void write_str(const char* s) {
        write_all(STDERR_FILENO, s, strlen(s));
    }

    void write_hex(std::uintptr_t value) {
        char buf[2 + sizeof(value) * 2];
        buf[0] = '0';
        buf[1] = 'x';
        for (std::size_t i = 0; i < sizeof(value) * 2; i++) {
            int nibble = (value >> (4 * (sizeof(value) * 2 - 1 - i))) & 0xF;
            buf[2 + i] = nibble < 10 ? static_cast<char>('0' + nibble) : static_cast<char>('a' + nibble - 10);
        }
        write_all(STDERR_FILENO, buf, sizeof(buf));
    }

    struct UnwindBuffer {
        cpptrace::frame_ptr* frames;
        std::size_t count;
        int skip;
    };

    _Unwind_Reason_Code unwind_frame(_Unwind_Context* ctx, void* arg) {
        auto& buf = *static_cast<UnwindBuffer*>(arg);
        int before_insn = 0;
        cpptrace::frame_ptr ip = _Unwind_GetIPInfo(ctx, &before_insn);
        if (ip == 0) {
            return _URC_END_OF_STACK;
        }
        if (buf.skip > 0) {
            buf.skip--;
            return _URC_NO_REASON;
        }
        buf.frames[buf.count++] = before_insn ? ip : ip - 1;
        return buf.count < MAX_FRAMES ? _URC_NO_REASON : _URC_END_OF_STACK;
    }

    const char* signal_name(int sig) {
        switch (sig) {
        case SIGSEGV:
            return "SIGSEGV (Segmentation fault)";
        case SIGBUS:
            return "SIGBUS (Bus error)";
        case SIGABRT:
            return "SIGABRT (Abort)";
        case SIGFPE:
            return "SIGFPE (Floating point exception)";
        case SIGILL:
            return "SIGILL (Illegal instruction)";
        default:
            return "Unknown signal";
        }
    }

    const char* fault_reason(int sig, int code) {
        if (sig == SIGSEGV) {
            return code == SEGV_MAPERR ? "not mapped" : code == SEGV_ACCERR ? "no permission" : nullptr;
        }
        if (sig == SIGBUS) {
            return code == BUS_ADRALN ? "misaligned" : code == BUS_ADRERR ? "no such address" : nullptr;
        }
        if (sig == SIGFPE) {
            return code == FPE_INTDIV ? "int divide by zero" : code == FPE_FLTDIV ? "float divide by zero" : nullptr;
        }
        return nullptr;
    }

    bool has_fault_address(int sig) {
        return sig == SIGSEGV || sig == SIGBUS || sig == SIGFPE || sig == SIGILL;
    }

    void crash_signal_handler(int sig, siginfo_t* info, void*) {
        if (g_crashing.test_and_set()) {
            _exit(1);
        }
        const char* name = signal_name(sig);
        const char* reason = fault_reason(sig, info->si_code);
        const auto addr = reinterpret_cast<std::uintptr_t>(info->si_addr);

        cpptrace::frame_ptr frames[MAX_FRAMES];
        UnwindBuffer buf{frames, 0, 2};
        _Unwind_Backtrace(unwind_frame, &buf);
        const std::size_t count = buf.count;
        write_str("Crash: ");
        write_str(name);
        if (has_fault_address(sig)) {
            write_str(", address ");
            write_hex(addr);
            if (reason) {
                write_str(" (");
                write_str(reason);
                write_str(")");
            }
        }
        write_str("\n");
        for (std::size_t i = 0; i < count; i++) {
            write_str("  ");
            write_hex(frames[i]);
            write_str("\n");
        }

        char thread[32] = "?";
        pthread_getname_np(pthread_self(), thread, sizeof(thread));
        if (has_fault_address(sig)) {
            spdlog::critical("Crash: {}, address {:#x}{}{}{}, thread '{}'", name, addr, reason ? " (" : "",
                             reason ? reason : "", reason ? ")" : "", thread);
        } else {
            spdlog::critical("Crash: {}, thread '{}'", name, thread);
        }
        cpptrace::raw_trace raw{std::vector<cpptrace::frame_ptr>(frames, frames + count)};
        spdlog::critical("{}", raw.resolve().to_string(false));
        spdlog::critical("Exiting with signal {}", sig);
        spdlog::default_logger()->flush();

        raise(sig);
        _exit(1);
    }

    void signal_handler(int signal) {
        if (signal == SIGINT) {
            _exit(0);
        }
    }

} // namespace

void platform_install_crash_handlers() {
    static std::vector<char> altstack(std::max<std::size_t>(SIGSTKSZ, 64 * 1024));
    stack_t ss{};
    ss.ss_sp = altstack.data();
    ss.ss_size = altstack.size();
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) != 0) {
        spdlog::warn("sigaltstack failed: {}", strerror(errno));
    }

    struct sigaction sa{};
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO | SA_RESETHAND | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL}) {
        if (sigaction(sig, &sa, nullptr) != 0) {
            spdlog::warn("sigaction({}) failed: {}", strsignal(sig), strerror(errno));
        }
    }

    struct sigaction sa_int{};
    sa_int.sa_handler = signal_handler;
    sigemptyset(&sa_int.sa_mask);
    if (sigaction(SIGINT, &sa_int, nullptr) != 0) {
        spdlog::warn("sigaction(SIGINT) failed: {}", strerror(errno));
    }
}

bool platform_retains_assets() {
    return true;
}
