add_executable(PokemonTrackerTest
  PokemonTrackerTest.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonTracker.cpp
)

target_link_libraries(PokemonTrackerTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

add_test(NAME PokemonTrackerTest COMMAND PokemonTrackerTest)
