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

add_executable(PokemonStoreCodecTest
  PokemonStoreCodecTest.cpp
  "${POKEMON_SPECIES_GENERATED_HEADER}"
  ${REPO_ROOT}/lib/Pokemon/PokemonTypes.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonSpecies.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonStoreCodec.cpp
)

target_include_directories(PokemonStoreCodecTest PRIVATE
  "${POKEMON_SPECIES_GENERATED_DIR}"
)

target_link_libraries(PokemonStoreCodecTest PRIVATE
  crosspoint_test_common
)

add_test(NAME PokemonStoreCodecTest COMMAND PokemonStoreCodecTest)

add_executable(PokemonStoreTest
  PokemonStoreTest.cpp
  "${POKEMON_SPECIES_GENERATED_HEADER}"
  ${REPO_ROOT}/lib/Pokemon/PokemonTypes.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonSpecies.cpp
  ${REPO_ROOT}/lib/Pokemon/PokemonStoreCodec.cpp
  ${REPO_ROOT}/src/pokemon/PokemonStore.cpp
)

target_include_directories(PokemonStoreTest PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
  "${POKEMON_SPECIES_GENERATED_DIR}"
  "${REPO_ROOT}/src"
)

target_link_libraries(PokemonStoreTest PRIVATE
  crosspoint_test_common
)

add_test(NAME PokemonStoreTest COMMAND PokemonStoreTest)
