add_executable(FootnoteCacheTest
  FootnoteCacheTest.cpp
)

target_include_directories(FootnoteCacheTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/EpdFont
)

target_link_libraries(FootnoteCacheTest PRIVATE
  crosspoint_test_common
)

add_test(NAME FootnoteCacheTest COMMAND FootnoteCacheTest)
