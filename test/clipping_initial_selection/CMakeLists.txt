add_executable(ClippingInitialSelectionTest
  ClippingInitialSelectionTest.cpp
)

target_include_directories(ClippingInitialSelectionTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/EpdFont
)

target_link_libraries(ClippingInitialSelectionTest PRIVATE
  crosspoint_test_common
)

add_test(NAME ClippingInitialSelectionTest COMMAND ClippingInitialSelectionTest)
