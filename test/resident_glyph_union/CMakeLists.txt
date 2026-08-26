add_executable(resident_glyph_union_test
  ResidentGlyphUnionTest.cpp
)

target_link_libraries(resident_glyph_union_test PRIVATE
  crosspoint_test_common
)

add_test(NAME resident_glyph_union_test COMMAND resident_glyph_union_test)
