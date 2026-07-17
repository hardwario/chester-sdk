
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

# The subsystem source is compiled directly into each suite via #include
# "ctr_trace.c" (white-box access to its now-static helpers), NOT linked through
# CONFIG_CTR_TRACE (kept off in prj.conf). This include path resolves both that
# include and its nested #include "ctr_trace_priv.h".
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_trace)

# Kconfig knobs the subsystem source needs when compiled directly: with
# CONFIG_CTR_TRACE=n the `if CTR_TRACE` block (log level, reason count, init
# priority, shell) is absent from autoconf, so supply them here. Keep in sync
# with subsys/ctr_trace/Kconfig defaults.
zephyr_compile_definitions(
  CONFIG_CTR_TRACE_LOG_LEVEL=4          # LOG_LEVEL_DBG
  CONFIG_CTR_TRACE_RESET_REASON_COUNT=8 # Kconfig default
  CONFIG_CTR_TRACE_INIT_PRIORITY=90     # = CONFIG_APPLICATION_INIT_PRIORITY default
  CONFIG_CTR_TRACE_SHELL=1              # registers the `trace` shell command set
)

# Mock retained_mem device (chosen device for the subsystem + scratch device
# for the unit tests).
target_sources(app PRIVATE src/mock_retmem.c)

# Test suites; one CONFIG_TEST_FEATURE_* flag each.
target_sources_ifdef(CONFIG_TEST_FEATURE_LAYOUT app PRIVATE src/test_layout.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_RING   app PRIVATE src/test_ring.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_REASON app PRIVATE src/test_reason.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_SHELL  app PRIVATE src/test_shell.c)
