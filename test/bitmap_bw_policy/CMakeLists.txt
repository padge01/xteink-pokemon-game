add_executable(BitmapBwPolicyTest
  BitmapBwPolicyTest.cpp
)

target_include_directories(BitmapBwPolicyTest PRIVATE
  ${REPO_ROOT}/lib/GfxRenderer
)

target_link_libraries(BitmapBwPolicyTest PRIVATE
  crosspoint_test_common
)

add_test(NAME BitmapBwPolicyTest COMMAND BitmapBwPolicyTest)
