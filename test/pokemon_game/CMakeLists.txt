find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(POKEMON_SPECIES_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(POKEMON_SPECIES_GENERATED_HEADER
    "${POKEMON_SPECIES_GENERATED_DIR}/PokemonSpecies.generated.h")

add_custom_command(
  OUTPUT "${POKEMON_SPECIES_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_SPECIES_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_v2_species.py"
          --input "${REPO_ROOT}/scripts/data/pokemon-kanto-v2.csv"
          --output "${POKEMON_SPECIES_GENERATED_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_v2_species.py"
    "${REPO_ROOT}/scripts/data/pokemon-kanto-v2.csv"
  VERBATIM
)

add_executable(PokemonGameTest
  PokemonGameTest.cpp
  "${POKEMON_SPECIES_GENERATED_HEADER}"
  ${REPO_ROOT}/lib/Pokemon/PokemonTypes.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonSpecies.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonGame.cpp
)

target_include_directories(PokemonGameTest PRIVATE
  "${POKEMON_SPECIES_GENERATED_DIR}"
)

target_link_libraries(PokemonGameTest PRIVATE
  crosspoint_test_common
)

add_test(NAME PokemonGameTest COMMAND PokemonGameTest)
