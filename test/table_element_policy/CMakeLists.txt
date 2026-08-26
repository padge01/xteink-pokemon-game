add_executable(TableElementPolicyTest
  TableElementPolicyTest.cpp
)

target_link_libraries(TableElementPolicyTest PRIVATE
  crosspoint_test_common
)

add_test(NAME TableElementPolicyTest COMMAND TableElementPolicyTest)
