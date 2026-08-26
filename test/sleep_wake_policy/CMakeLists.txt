add_executable(SleepWakePolicyTest
  SleepWakePolicyTest.cpp
)

target_link_libraries(SleepWakePolicyTest PRIVATE
  crosspoint_test_common
)

add_test(NAME SleepWakePolicyTest COMMAND SleepWakePolicyTest)
