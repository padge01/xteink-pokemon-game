add_executable(ImagePlacementPolicyTest
  ImagePlacementPolicyTest.cpp
)

target_link_libraries(ImagePlacementPolicyTest PRIVATE
  crosspoint_test_common
)

add_test(NAME ImagePlacementPolicyTest COMMAND ImagePlacementPolicyTest)
