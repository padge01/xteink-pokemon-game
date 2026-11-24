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

add_executable(PokemonServiceTest
  PokemonServiceTest.cpp
  "${POKEMON_SERVICE_SPECIES_HEADER}"
  ${REPO_ROOT}/lib/Pokemon/PokemonTypes.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonSpecies.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonGame.cpp
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
