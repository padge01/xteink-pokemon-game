add_executable(ManualPageTurnGuardTest
  ManualPageTurnGuardTest.cpp
)

target_link_libraries(ManualPageTurnGuardTest PRIVATE
  crosspoint_test_common
)

add_test(NAME ManualPageTurnGuardTest COMMAND ManualPageTurnGuardTest)
