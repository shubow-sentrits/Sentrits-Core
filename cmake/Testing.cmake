include(CTest)
include(${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake)

if(TARGET GTest::gtest_main)
  set(SENTRITS_GTEST_MAIN_TARGET GTest::gtest_main)
elseif(TARGET gtest_main)
  set(SENTRITS_GTEST_MAIN_TARGET gtest_main)
else()
  message(FATAL_ERROR "googletest main target was not created")
endif()

add_executable(sentrits_tests
  ${SENTRITS_TEST_SOURCES}
)

add_executable(sentrits_session_start_fixture
  tests/helpers/session_start_fixture.cpp
)

add_executable(sentrits_supervision_probe
  src/tools/supervision_probe.cpp
)

target_link_libraries(sentrits_supervision_probe
  PRIVATE
    vibe_core
)

# StackExpectation uses designated initializers with sparse field sets.
# -Wmissing-designated-field-initializers is too strict for a dev probe tool.
target_compile_options(sentrits_supervision_probe PRIVATE
  -Wno-missing-designated-field-initializers
)

add_dependencies(sentrits_tests
  sentrits
  sentrits_session_start_fixture
)

target_link_libraries(sentrits_tests
  PRIVATE
    vibe_core
    ${SENTRITS_GTEST_MAIN_TARGET}
)

target_compile_definitions(sentrits_tests
  PRIVATE
    VIBE_HOSTD_PATH="$<TARGET_FILE:sentrits>"
    SENTRITS_SESSION_START_FIXTURE_PATH="$<TARGET_FILE:sentrits_session_start_fixture>"
)

include(GoogleTest)
gtest_discover_tests(sentrits_tests TEST_LIST SENTRITS_DISCOVERED_TESTS)
set_tests_properties(${SENTRITS_DISCOVERED_TESTS} PROPERTIES RUN_SERIAL TRUE)
