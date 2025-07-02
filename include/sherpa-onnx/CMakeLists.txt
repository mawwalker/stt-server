add_subdirectory(csrc)

if(SHERPA_ONNX_ENABLE_C_API)
  add_subdirectory(c-api)
endif()
