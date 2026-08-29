add_executable(PokemonTypesTest
  PokemonTypesTest.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonTypes.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonSpecies.cpp
)

target_link_libraries(PokemonTypesTest PRIVATE
  crosspoint_test_common
)

add_test(NAME PokemonTypesTest COMMAND PokemonTypesTest)
