find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(POKEMON_BATTLE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(POKEMON_SPECIES_GENERATED_HEADER "${POKEMON_BATTLE_GENERATED_DIR}/PokemonSpecies.generated.h")
set(POKEMON_MOVES_GENERATED_HEADER "${POKEMON_BATTLE_GENERATED_DIR}/PokemonMoves.generated.h")
set(POKEMON_STATS_GENERATED_HEADER "${POKEMON_BATTLE_GENERATED_DIR}/PokemonStats.generated.h")
set(POKEMON_LEARNSETS_GENERATED_HEADER "${POKEMON_BATTLE_GENERATED_DIR}/PokemonLearnsets.generated.h")
set(POKEMON_ITEMS_GENERATED_HEADER "${POKEMON_BATTLE_GENERATED_DIR}/PokemonItems.generated.h")
set(POKEMON_GYMS_GENERATED_HEADER "${POKEMON_BATTLE_GENERATED_DIR}/PokemonGyms.generated.h")

add_custom_command(
  OUTPUT "${POKEMON_SPECIES_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_BATTLE_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_v2_species.py"
          --input "${REPO_ROOT}/scripts/data/pokemon-kanto-v2.csv"
          --output "${POKEMON_SPECIES_GENERATED_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_v2_species.py"
    "${REPO_ROOT}/scripts/data/pokemon-kanto-v2.csv"
  VERBATIM
)

add_custom_command(
  OUTPUT "${POKEMON_MOVES_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_BATTLE_GENERATED_DIR}"
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
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_BATTLE_GENERATED_DIR}"
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
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_BATTLE_GENERATED_DIR}"
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

add_custom_command(
  OUTPUT "${POKEMON_ITEMS_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_BATTLE_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_items.py"
          --input "${REPO_ROOT}/scripts/data/pokemon-items.csv"
          --output "${POKEMON_ITEMS_GENERATED_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_items.py"
    "${REPO_ROOT}/scripts/data/pokemon-items.csv"
  VERBATIM
)

add_custom_command(
  OUTPUT "${POKEMON_GYMS_GENERATED_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${POKEMON_BATTLE_GENERATED_DIR}"
  COMMAND ${Python3_EXECUTABLE}
          "${REPO_ROOT}/scripts/generate_pokemon_gyms.py"
          --input "${REPO_ROOT}/scripts/data/pokemon-gyms.csv"
          --output "${POKEMON_GYMS_GENERATED_HEADER}"
  DEPENDS
    "${REPO_ROOT}/scripts/generate_pokemon_gyms.py"
    "${REPO_ROOT}/scripts/data/pokemon-gyms.csv"
  VERBATIM
)

add_executable(PokemonBattleDataTest
  PokemonBattleDataTest.cpp
  "${POKEMON_SPECIES_GENERATED_HEADER}"
  "${POKEMON_MOVES_GENERATED_HEADER}"
  "${POKEMON_STATS_GENERATED_HEADER}"
  "${POKEMON_LEARNSETS_GENERATED_HEADER}"
  "${POKEMON_ITEMS_GENERATED_HEADER}"
  "${POKEMON_GYMS_GENERATED_HEADER}"
  ${REPO_ROOT}/lib/Pokemon/PokemonTypes.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonSpecies.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonMoveData.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonBaseStats.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonLearnsets.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonItemData.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonGymData.cpp
)

target_include_directories(PokemonBattleDataTest PRIVATE
  "${POKEMON_BATTLE_GENERATED_DIR}"
)

target_link_libraries(PokemonBattleDataTest PRIVATE
  crosspoint_test_common
)

add_test(NAME PokemonBattleDataTest COMMAND PokemonBattleDataTest)

add_test(
  NAME PokemonBattleDataGeneratorsTest
  COMMAND ${Python3_EXECUTABLE}
          "${CMAKE_CURRENT_SOURCE_DIR}/PokemonBattleDataGeneratorsTest.py"
          "${REPO_ROOT}"
)
