add_executable(ImageDecoderSafetyTest
  ImageDecoderSafetyTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/ImageDimsProbe.cpp
  ${REPO_ROOT}/lib/Epub/Epub/converters/ImageToFramebufferDecoder.cpp
)

target_include_directories(ImageDecoderSafetyTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/Epub/Epub/converters
)

target_link_libraries(ImageDecoderSafetyTest PRIVATE
  crosspoint_test_common
)

add_test(NAME ImageDecoderSafetyTest COMMAND ImageDecoderSafetyTest)
