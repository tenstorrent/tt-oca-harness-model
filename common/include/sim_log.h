// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sim_log.h
 * @brief Ergonomic, leveled logging for SystemC/TLM models — a thin layer over
 *        the IEEE-1666 `sc_report` facility (usable by any subsystem's models:
 *        SMC, SEP, VP, fabric, CPU cluster, ...).
 *
 * ## Why this exists
 *
 * Models today call `SC_REPORT_FATAL/INFO/WARNING` directly. That works for
 * fatal config checks and one-line construction summaries, but it gives no
 * leveled debug/trace tier, no per-transaction tracing, no stream syntax, and
 * an inconsistent message-id convention (each site passes raw `name()`).
 *
 * This header adds those affordances **without replacing `sc_report`**, so:
 *   - existing `sc_report_handler` setup, `set_actions()`, log files, and the
 *     testbench suppression patterns keep working unchanged;
 *   - levels map onto SystemC's native verbosity threshold, so
 *     `sc_report_handler::set_verbosity_level()` (or `simlog::set_level()`)
 *     turns detail up/down at run time;
 *   - INFO/DEBUG/TRACE below the active threshold cost only an integer compare
 *     (the message string is never built), and can be *compiled out* entirely
 *     via `-DSIM_LOG_LEVEL=n`.
 *
 * ## Dependency
 *
 * Depends only on SystemC (already a universal dependency of every model) and
 * `<sstream>`. It intentionally does NOT depend on CCI; a model that wants a
 * CCI-driven log knob can call `simlog::set_level()` from a `cci_param`
 * post-write callback (see the example at the bottom).
 *
 * ## Levels ⇄ SystemC verbosity
 *
 * | simlog::level | sc_report                         | verbosity  |
 * |---------------|-----------------------------------|------------|
 * | error         | `SC_ERROR`  (not verbosity-gated) | —          |
 * | warning       | `SC_WARNING`(not verbosity-gated) | —          |
 * | info          | `SC_INFO`                         | `SC_MEDIUM`|
 * | debug         | `SC_INFO`                         | `SC_HIGH`  |
 * | trace         | `SC_INFO`                         | `SC_FULL`  |
 *
 * Note: only `SC_INFO` reports are verbosity-gated in SystemC. Errors/warnings
 * always emit (subject to `set_actions`), matching current model behavior.
 *
 * ## Usage
 *
 * @code
 *   #include "sim_log.h"
 *   // inside an sc_module method (this is an sc_object*):
 *   SIM_LOG_INFO (this, "configured: " << depth << " entries");
 *   SIM_LOG_DEBUG(this, "reg write off=0x" << std::hex << off << " data=0x" << data);
 *   SIM_LOG_TRACE(this, "b_transport enter, delay=" << delay);
 *   SIM_LOG_WARN (this, "back-door index " << i << " out of range");
 *   SIM_LOG_ERROR(this, "decode miss at 0x" << std::hex << addr);
 *   // a plain category string also works as the id:
 *   SIM_LOG_INFO ("smc.fabric", "route table rebuilt");
 * @endcode
 *
 * Turn detail up at run time (e.g. in a testbench or from a CCI callback):
 * @code
 *   simlog::set_level(simlog::level::debug);
 * @endcode
 */

#pragma once

#include <sstream>
#include <string>

#include <systemc>

namespace simlog {

/// Logical logging levels, ordered by increasing detail.
enum class level : int {
    off     = 0,
    error   = 1,
    warning = 2,
    info    = 3,
    debug   = 4,
    trace   = 5,
};

namespace detail {

/// Resolve the `sc_report` "message type" (id) from whatever the caller passed.
/// Accepting an `sc_object*`/reference means `this` inside a module Just Works
/// and the report is tagged with the instance's hierarchical name.
inline const char* id_of(const ::sc_core::sc_object* o) { return o ? o->name() : "sim"; }
inline const char* id_of(const ::sc_core::sc_object& o) { return o.name(); }
inline const char* id_of(const char* s)                 { return s ? s : "sim"; }
inline const char* id_of(const ::std::string& s)        { return s.c_str(); }

} // namespace detail

/// Set the run-time detail threshold (maps to SystemC's verbosity level).
/// INFO shows at `info`+, DEBUG at `debug`+, TRACE at `trace`. Errors and
/// warnings are never gated by this (they are gated by `set_actions`).
inline void set_level(level l)
{
    int v = ::sc_core::SC_MEDIUM;
    switch (l) {
        case level::off:                                   // fallthrough
        case level::error:                                 // fallthrough
        case level::warning: v = ::sc_core::SC_NONE;   break; // no INFO at all
        case level::info:    v = ::sc_core::SC_MEDIUM; break;
        case level::debug:   v = ::sc_core::SC_HIGH;   break;
        case level::trace:   v = ::sc_core::SC_FULL;   break;
    }
    ::sc_core::sc_report_handler::set_verbosity_level(v);
}

/// Current SystemC verbosity threshold (for callers that want to gate manually).
inline int current_verbosity()
{
    return ::sc_core::sc_report_handler::get_verbosity_level();
}

} // namespace simlog

// ---------------------------------------------------------------------------
// Compile-time level: everything above SIM_LOG_LEVEL is removed by the
// preprocessor (zero code, zero cost). Default = trace (everything kept).
// Build a release with e.g. -DSIM_LOG_LEVEL=3 to strip debug/trace entirely.
//   1=error 2=warning 3=info 4=debug 5=trace
// ---------------------------------------------------------------------------
#ifndef SIM_LOG_LEVEL
#define SIM_LOG_LEVEL 5
#endif

/// @cond INTERNAL
// Emit a verbosity-gated SC_INFO. The string is built only if the report would
// actually be shown, so sub-threshold logs cost one integer compare.
#define SIM_LOG_DETAIL_INFO_(idexpr, verb, msg)                                 \
    do {                                                                        \
        if ((verb) <= ::sc_core::sc_report_handler::get_verbosity_level()) {    \
            ::std::ostringstream sim_log_oss_;                                   \
            sim_log_oss_ << msg;                                                 \
            ::sc_core::sc_report_handler::report(                               \
                ::sc_core::SC_INFO, ::simlog::detail::id_of(idexpr),             \
                sim_log_oss_.str().c_str(), (verb), __FILE__, __LINE__);         \
        }                                                                       \
    } while (false)

// Emit a non-gated report (error/warning/fatal).
#define SIM_LOG_DETAIL_SEV_(idexpr, sev, msg)                                   \
    do {                                                                        \
        ::std::ostringstream sim_log_oss_;                                       \
        sim_log_oss_ << msg;                                                     \
        ::sc_core::sc_report_handler::report(                                   \
            (sev), ::simlog::detail::id_of(idexpr),                             \
            sim_log_oss_.str().c_str(), __FILE__, __LINE__);                     \
    } while (false)
/// @endcond

// ---------------------------------------------------------------------------
// Public macros. First arg is the log id: an `sc_object*` (typically `this`),
// an `sc_object&`, or a category string. Second arg is a `<<`-stream expression.
// ---------------------------------------------------------------------------

/// Fatal: always compiled in; aborts simulation (config/programmer errors).
#define SIM_LOG_FATAL(obj, msg) SIM_LOG_DETAIL_SEV_(obj, ::sc_core::SC_FATAL, msg)

#if SIM_LOG_LEVEL >= 1
#define SIM_LOG_ERROR(obj, msg) SIM_LOG_DETAIL_SEV_(obj, ::sc_core::SC_ERROR, msg)
#else
#define SIM_LOG_ERROR(obj, msg) do {} while (false)
#endif

#if SIM_LOG_LEVEL >= 2
#define SIM_LOG_WARN(obj, msg) SIM_LOG_DETAIL_SEV_(obj, ::sc_core::SC_WARNING, msg)
#else
#define SIM_LOG_WARN(obj, msg) do {} while (false)
#endif

#if SIM_LOG_LEVEL >= 3
#define SIM_LOG_INFO(obj, msg) SIM_LOG_DETAIL_INFO_(obj, ::sc_core::SC_MEDIUM, msg)
#else
#define SIM_LOG_INFO(obj, msg) do {} while (false)
#endif

#if SIM_LOG_LEVEL >= 4
#define SIM_LOG_DEBUG(obj, msg) SIM_LOG_DETAIL_INFO_(obj, ::sc_core::SC_HIGH, msg)
#else
#define SIM_LOG_DEBUG(obj, msg) do {} while (false)
#endif

#if SIM_LOG_LEVEL >= 5
#define SIM_LOG_TRACE(obj, msg) SIM_LOG_DETAIL_INFO_(obj, ::sc_core::SC_FULL, msg)
#else
#define SIM_LOG_TRACE(obj, msg) do {} while (false)
#endif

/*
 * Optional CCI wiring (kept out of this header to avoid a CCI dependency):
 *
 *   // in the module, with a cci::cci_param<std::string> log_level_p_:
 *   auto apply = [this]{
 *       const auto& s = log_level_p_.get_value();
 *       if      (s == "trace") simlog::set_level(simlog::level::trace);
 *       else if (s == "debug") simlog::set_level(simlog::level::debug);
 *       else                    simlog::set_level(simlog::level::info);
 *   };
 *   apply();
 *   log_level_p_.register_post_write_callback([apply](auto&){ apply(); });
 */
