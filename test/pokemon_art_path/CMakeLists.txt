add_executable(PokemonArtPathTest
  PokemonArtPathTest.cpp
  ${REPO_ROOT}/src/components/pokemon/PokemonArtPath.cpp
)

target_include_directories(PokemonArtPathTest PRIVATE ${REPO_ROOT}/src)

target_link_libraries(PokemonArtPathTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PokemonArtPathTest)
