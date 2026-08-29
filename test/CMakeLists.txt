cmake_minimum_required(VERSION 3.16)
project(crosspoint_reader_tests C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_library(crosspoint_test_common INTERFACE)
target_include_directories(crosspoint_test_common INTERFACE
  ${REPO_ROOT}
  ${REPO_ROOT}/lib
)
target_compile_options(crosspoint_test_common INTERFACE
  -Wall
  -Wextra
  -pedantic
)

enable_testing()
include(GoogleTest)

add_subdirectory(streaming_json_parser)
add_subdirectory(release_json_parser)
add_subdirectory(differential_rounding)
add_subdirectory(hyphenation_eval)
add_subdirectory(utf8_compose)
add_subdirectory(minibidi_arabic)
add_subdirectory(combining_marks)
add_subdirectory(preview_block_locator)
add_subdirectory(text_pool)
add_subdirectory(clip_word_store)
add_subdirectory(dict_layout)
add_subdirectory(reader_progress_save_debouncer)
add_subdirectory(koreader_sync_http_status)
add_subdirectory(button_hint_layout)
add_subdirectory(footnote_cache)
add_subdirectory(manual_page_turn_guard)
add_subdirectory(sleep_wake_policy)
add_subdirectory(directory_iteration_result)
add_subdirectory(clipping_highlight_geometry)
add_subdirectory(dictionary_lookup_policy)
add_subdirectory(reordered_width_scratch)
add_subdirectory(button_navigator)
add_subdirectory(font_cache_manager)
add_subdirectory(resident_glyph_union)
add_subdirectory(table_element_policy)
add_subdirectory(clipping_initial_selection)
add_subdirectory(deferred_reposition_state)
add_subdirectory(html_entities)
add_subdirectory(xml_parser_utils)
add_subdirectory(token_boundary)
add_subdirectory(long_text_run_flush_policy)
add_subdirectory(image_decoder_safety)
add_subdirectory(image_placement_policy)
add_subdirectory(pokemon_types)
