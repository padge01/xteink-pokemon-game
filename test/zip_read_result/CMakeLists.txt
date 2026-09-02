add_executable(ZipReadResultTest
  ZipReadResultTest.cpp
)

target_include_directories(ZipReadResultTest PRIVATE
  ${REPO_ROOT}/lib/ZipFile
)

target_link_libraries(ZipReadResultTest PRIVATE
  crosspoint_test_common
)

add_test(NAME ZipReadResultTest COMMAND ZipReadResultTest)
