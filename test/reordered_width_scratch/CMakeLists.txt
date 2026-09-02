add_executable(ReorderedWidthScratchTest
  ReorderedWidthScratchTest.cpp
)

target_link_libraries(ReorderedWidthScratchTest PRIVATE
  crosspoint_test_common
)

add_test(NAME ReorderedWidthScratchTest COMMAND ReorderedWidthScratchTest)
