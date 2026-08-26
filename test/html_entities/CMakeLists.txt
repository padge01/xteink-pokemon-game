add_executable(HtmlEntitiesTest
  HtmlEntitiesTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
)

target_include_directories(HtmlEntitiesTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub
)

target_link_libraries(HtmlEntitiesTest PRIVATE
  crosspoint_test_common
)

add_test(NAME HtmlEntitiesTest COMMAND HtmlEntitiesTest)
