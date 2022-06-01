set(ALL_FILES

  cxxopts.hpp
  log.cpp
  log.h
  main_win.cpp
  nvenc.cpp
  nvenc.h
  pch.h
  resources.rc
  streaming/streaming.h
  streaming/webrtc.cpp
  streaming/webrtc.h
  trace_logging.cpp
  trace_logging.h
  version.h

  video_input/device.h
  video_input/media_foundation.cpp
  video_input/media_foundation.h

  nlohmann/json.hpp

  NvEncoder/NvEncoder.cpp
  NvEncoder/NvEncoder.h
  NvEncoder/NvEncoderD3D11.cpp
  NvEncoder/NvEncoderD3D11.h
  NvEncoder/RGBToNV12ConverterD3D11.h
)

foreach(FILE ${ALL_FILES}) 
  get_filename_component(PARENT_DIR "${FILE}" PATH)
  string(REPLACE "/" "\\" GROUP "${PARENT_DIR}")
  source_group("${GROUP}" FILES "${FILE}")
endforeach()

add_executable(server ${ALL_FILES})

target_include_directories(server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_precompile_headers(server PRIVATE pch.h)

find_package(NvVideoCodecSDK REQUIRED)

find_package(LibDataChannel CONFIG REQUIRED)

find_package(civetweb CONFIG REQUIRED) 

if(MSVC)
  target_link_libraries(server PRIVATE "d3d11" "dxguid" "dxgi" "mfplat" "mf" "mfreadwrite" "mfuuid" "ws2_32" LibDataChannel::LibDataChannel)
  target_link_libraries(server PRIVATE NvVideoCodecSDK::NvVideoCodecSDK)
  target_link_libraries(server PRIVATE civetweb::civetweb civetweb::civetweb-cpp)

  set_target_properties(server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()

