add_executable(font_cache_manager_test
  FontCacheManagerTest.cpp
  ${REPO_ROOT}/lib/GfxRenderer/FontCacheManager.cpp
)

target_include_directories(font_cache_manager_test BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/GfxRenderer
)

target_link_libraries(font_cache_manager_test PRIVATE
  crosspoint_test_common
)

add_test(NAME font_cache_manager_test COMMAND font_cache_manager_test)
