# Throughput benchmarks. Built only when QTERM_BUILD_BENCHMARKS is ON, so a
# normal `ctest` run never includes them.
#
# They report numbers; they do not assert them. A wall-clock threshold would
# have to be retuned per machine and per build type (a Debug build is ~2.5x
# slower), and the failures it produced would be noise rather than signal. The
# properties worth failing on are invariants, and those live in the ordinary
# tests -- see widget/QTermWidgetRepaintTest.cpp, which catches the class of
# regression that a timing threshold is bad at catching.

set(QTERM_BENCHMARK_PAYLOAD_DIR "/tmp/qterm-bench"
    CACHE PATH "Where make_payloads.sh writes the benchmark payloads")

# ── coremem: parser only, no PTY and no rendering ────────────────────────────
qt_add_executable(qterm_bench_coremem coremem.cpp)
target_include_directories(qterm_bench_coremem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(qterm_bench_coremem PRIVATE Qt6::Core QTerm::qterm)

# ── shaping: full text shaping vs glyph-index lookup ─────────────────────────
qt_add_executable(qterm_bench_shaping shaping.cpp)
target_link_libraries(qterm_bench_shaping PRIVATE Qt6::Core Qt6::Gui QTerm::qterm)

# ── realtime: last line on screen, Qt Quick scene graph ──────────────────────
qt_add_executable(qterm_bench_realtime realtime.cpp)
target_link_libraries(qterm_bench_realtime PRIVATE Qt6::Core Qt6::Gui Qt6::Quick QTerm::qterm)

# ── realtime_widget: same yardstick on the widget path ───────────────────────
qt_add_executable(qterm_bench_realtime_widget realtime_widget.cpp)
target_link_libraries(qterm_bench_realtime_widget
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets QTerm::qterm)

# ── ctest wiring ─────────────────────────────────────────────────────────────
#
# The payloads are hundreds of megabytes and take a while to generate, so they
# are produced once by a fixture the benchmarks depend on rather than by each
# run. make_payloads.sh skips whatever already exists.
add_test(NAME qterm_bench_payloads
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/make_payloads.sh
                 ${QTERM_BENCHMARK_PAYLOAD_DIR})
set_tests_properties(qterm_bench_payloads PROPERTIES
    FIXTURES_SETUP qterm_payloads
    LABELS benchmark
    TIMEOUT 900)

# payload file, marker that identifies its last line
set(QTERM_BENCH_CASES
    "lines1m.txt|999999"
    "color1m.txt|segment-199999"
    "mixed.txt|seg-299999"
    "cjk.txt|第399999行"
)

foreach(case IN LISTS QTERM_BENCH_CASES)
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 payload)
    list(GET parts 1 marker)
    string(REGEX REPLACE "\\.txt$" "" name "${payload}")

    foreach(variant realtime realtime_widget)
        add_test(NAME qterm_bench_${variant}_${name}
                 COMMAND qterm_bench_${variant}
                         ${QTERM_BENCHMARK_PAYLOAD_DIR}/${payload} ${marker})
        set_tests_properties(qterm_bench_${variant}_${name} PROPERTIES
            FIXTURES_REQUIRED qterm_payloads
            LABELS benchmark
            # Both renderers must run at the same window size, or the grids
            # differ and the two columns of numbers are not comparable. The
            # tools default to different sizes, so pin them here.
            ENVIRONMENT "QTERM_W=1252;QTERM_H=760"
            # Generous: this only catches a hang, it is not a performance bound.
            TIMEOUT 300
            # Benchmarks measure what reaches the screen, so they need a real
            # window; a headless runner has to skip them.
            RUN_SERIAL TRUE)
    endforeach()
endforeach()

add_test(NAME qterm_bench_coremem
         COMMAND qterm_bench_coremem ${QTERM_BENCHMARK_PAYLOAD_DIR}/color1m_crlf.txt)
set_tests_properties(qterm_bench_coremem PROPERTIES
    FIXTURES_REQUIRED qterm_payloads
    LABELS benchmark
    TIMEOUT 300)
