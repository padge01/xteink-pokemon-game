add_executable(ReaderMenuItemsTest
  ReaderMenuItemsTest.cpp
  ${REPO_ROOT}/src/activities/reader/ReaderMenuItems.cpp
)

target_include_directories(ReaderMenuItemsTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/I18n
)

target_link_libraries(ReaderMenuItemsTest PRIVATE crosspoint_test_common)

add_test(NAME ReaderMenuItems COMMAND ReaderMenuItemsTest)
