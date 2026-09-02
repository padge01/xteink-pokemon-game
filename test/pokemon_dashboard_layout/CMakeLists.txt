add_executable(PokemonDashboardLayoutTest
  PokemonDashboardLayoutTest.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonDashboardLayout.cpp
)

target_link_libraries(PokemonDashboardLayoutTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

add_test(NAME PokemonDashboardLayoutTest COMMAND PokemonDashboardLayoutTest)
