//=-- lsan.cpp ------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of LeakSanitizer.
// Standalone LSan RTL.
//
//===----------------------------------------------------------------------===//

#include "lsan.h"

#include "lsan_allocator.h"
#include "lsan_common.h"
#include "lsan_thread.h"
#include "sanitizer_common/sanitizer_flag_parser.h"
#include "sanitizer_common/sanitizer_flags.h"
#include "sanitizer_common/sanitizer_interface_internal.h"

#if SANITIZER_EMSCRIPTEN
#include "emscripten_internal.h"
#include <emscripten/heap.h>
#endif

bool lsan_inited;
bool lsan_init_is_running;

namespace __lsan {

///// Interface to the common LSan module. /////
bool WordIsPoisoned(uptr addr) {
  return false;
}

}  // namespace __lsan

void __sanitizer::BufferedStackTrace::UnwindImpl(
    uptr pc, uptr bp, void *context, bool request_fast, u32 max_depth) {
  using namespace __lsan;
  uptr stack_top = 0, stack_bottom = 0;
  if (ThreadContextLsanBase *t = GetCurrentThread()) {
    stack_top = t->stack_end();
    stack_bottom = t->stack_begin();
  }
  if (SANITIZER_MIPS && !IsValidFrame(bp, stack_top, stack_bottom))
    return;
  bool fast = StackTrace::WillUseFastUnwind(request_fast);
  Unwind(max_depth, pc, bp, context, stack_top, stack_bottom, fast);
}

using namespace __lsan;

static void InitializeFlags() {
  // Set all the default values.
  SetCommonFlagsDefaults();
  {
    CommonFlags cf;
    cf.CopyFrom(*common_flags());
#if !SANITIZER_EMSCRIPTEN
    // getenv on emscripten uses malloc, which we can't when using LSan.
    // You can't run external symbolizers anyway.
    cf.external_symbolizer_path = GetEnv("LSAN_SYMBOLIZER_PATH");
#endif
    cf.malloc_context_size = 30;
    cf.intercept_tls_get_addr = true;
    cf.detect_leaks = true;
    cf.exitcode = 23;
    OverrideCommonFlags(cf);
  }

  Flags *f = flags();
  f->SetDefaults();

  FlagParser parser;
  RegisterLsanFlags(&parser, f);
  RegisterCommonFlags(&parser);

  // Override from user-specified string.
  const char *lsan_default_options = __lsan_default_options();
  parser.ParseString(lsan_default_options);
#if SANITIZER_EMSCRIPTEN
  char *options = _emscripten_sanitizer_get_option("LSAN_OPTIONS");
  parser.ParseString(options);
  emscripten_builtin_free(options);
#else
  parser.ParseString(GetEnv("LSAN_OPTIONS"));
#endif // SANITIZER_EMSCRIPTEN

#if SANITIZER_EMSCRIPTEN
  if (common_flags()->malloc_context_size <= 1)
    StackTrace::snapshot_stack = false;
#endif // SANITIZER_EMSCRIPTEN

  InitializeCommonFlags();

  if (Verbosity()) ReportUnrecognizedFlags();

  if (common_flags()->help) parser.PrintFlagDescriptions();

  __sanitizer_set_report_path(common_flags()->log_path);
}

extern "C" void __lsan_init() {
  CHECK(!lsan_init_is_running);
  if (lsan_inited)
    return;
  lsan_init_is_running = true;
  SanitizerToolName = "LeakSanitizer";
  CacheBinaryName();
  AvoidCVE_2016_2143();
  InitializeFlags();
  InitializePlatformEarly();
  InitCommonLsan();
  InitializeAllocator();
  ReplaceSystemMalloc();
  InitializeInterceptors();
  InitializeThreads();
#if !SANITIZER_EMSCRIPTEN
  // Emscripten does not have signals
  InstallDeadlySignalHandlers(LsanOnDeadlySignal);
#endif
  InitializeMainThread();
  InstallAtExitCheckLeaks();
  InstallAtForkHandler();

  InitializeCoverage(common_flags()->coverage, common_flags()->coverage_dir);

  lsan_inited = true;
  lsan_init_is_running = false;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __sanitizer_print_stack_trace() {
  GET_STACK_TRACE_FATAL;
  stack.Print();
}
