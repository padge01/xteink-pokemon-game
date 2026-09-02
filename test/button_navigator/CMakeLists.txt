add_executable(button_navigator_test
  ButtonNavigatorTest.cpp
  ${REPO_ROOT}/src/util/ButtonNavigator.cpp
)

target_include_directories(button_navigator_test BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
)

target_link_libraries(button_navigator_test PRIVATE
  crosspoint_test_common
)

add_test(NAME button_navigator_test COMMAND button_navigator_test)
