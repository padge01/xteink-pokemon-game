find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(POKEMON_SERVICE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(POKEMON_SERVICE_SPECIES_HEADER
    "${POKEMON_SERVICE_GENERATED_DIR}/PokemonSpecies.generated.h")

add_custom_command(
  OUTPUT "${POKEMON_SERVICE_SPECIES_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_SERVICE_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_v2_species.py"
          --input "${REPO_ROOT}/scripts/data/pokemon-kanto-v2.csv"
          --output "${POKEMON_SERVICE_SPECIES_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_v2_species.py"
    "${REPO_ROOT}/scripts/data/pokemon-kanto-v2.csv"
  VERBATIM
)

# The game rules pick a Pokemon's moves from the battle data.
set(POKEMON_MOVES_GENERATED_HEADER "${POKEMON_SERVICE_GENERATED_DIR}/PokemonMoves.generated.h")
set(POKEMON_STATS_GENERATED_HEADER "${POKEMON_SERVICE_GENERATED_DIR}/PokemonStats.generated.h")
set(POKEMON_LEARNSETS_GENERATED_HEADER "${POKEMON_SERVICE_GENERATED_DIR}/PokemonLearnsets.generated.h")

add_custom_command(
  OUTPUT "${POKEMON_MOVES_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_SERVICE_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_moves.py"
          --input "${REPO_ROOT}/scripts/data/pokemon-moves.csv"
          --output "${POKEMON_MOVES_GENERATED_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_moves.py"
    "${REPO_ROOT}/scripts/data/pokemon-moves.csv"
  VERBATIM
)

add_custom_command(
  OUTPUT "${POKEMON_STATS_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_SERVICE_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_stats.py"
          --input "${REPO_ROOT}/scripts/data/pokemon-stats.csv"
          --output "${POKEMON_STATS_GENERATED_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_stats.py"
    "${REPO_ROOT}/scripts/data/pokemon-stats.csv"
  VERBATIM
)

add_custom_command(
  OUTPUT "${POKEMON_LEARNSETS_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_SERVICE_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_learnsets.py"
          --learnsets-input "${REPO_ROOT}/scripts/data/pokemon-learnsets.csv"
          --tmhm-input "${REPO_ROOT}/scripts/data/pokemon-tmhm.csv"
          --output "${POKEMON_LEARNSETS_GENERATED_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_learnsets.py"
    "${REPO_ROOT}/scripts/data/pokemon-learnsets.csv"
    "${REPO_ROOT}/scripts/data/pokemon-tmhm.csv"
  VERBATIM
)

add_executable(PokemonServiceTest
  PokemonServiceTest.cpp
  "${POKEMON_SERVICE_SPECIES_HEADER}"
  "${POKEMON_MOVES_GENERATED_HEADER}"
  "${POKEMON_STATS_GENERATED_HEADER}"
  "${POKEMON_LEARNSETS_GENERATED_HEADER}"
  ${REPO_ROOT}/lib/Pokemon/PokemonTypes.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonSpecies.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonGame.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonMoveData.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonBaseStats.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonLearnsets.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonTypeChart.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonBattle.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonStoreCodec.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonTracker.cpp
  ${REPO_ROOT}/src/pokemon/PokemonStore.cpp
  ${REPO_ROOT}/src/pokemon/PokemonService.cpp
)

target_include_directories(PokemonServiceTest PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../pokemon_store/stubs"
  "${POKEMON_SERVICE_GENERATED_DIR}"
  "${REPO_ROOT}/src"
)

target_compile_definitions(PokemonServiceTest PRIVATE
  CROSSINK_ENABLE_POKEMON=1
  POKEMON_SERVICE_HOST_TEST=1
)

target_link_libraries(PokemonServiceTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

add_test(NAME PokemonServiceTest COMMAND PokemonServiceTest)
