add_executable(ButtonHintLayoutTest
  ButtonHintLayoutTest.cpp
)

target_link_libraries(ButtonHintLayoutTest PRIVATE
  crosspoint_test_common
)

target_include_directories(ButtonHintLayoutTest PRIVATE
  ${REPO_ROOT}/lib/EpdFont
)

add_test(NAME ButtonHintLayoutTest COMMAND ButtonHintLayoutTest)
