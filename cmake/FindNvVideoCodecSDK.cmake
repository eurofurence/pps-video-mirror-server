
# Find the NVidia Video Codec SDK
# Tries to use the environment variable NVIDIA_VIDEO_CODEC_SDK to find the necessary files (in addition to the usual paths)

# early out, if this target has been created before
if (TARGET NvVideoCodecSDK::NvVideoCodecSDK)
	return()
endif()

find_path(NV_VIDEO_CODEC_SDK_DIR Interface/nvEncodeAPI.h
  PATHS
  $ENV{NVIDIA_VIDEO_CODEC_SDK}
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(NVIDIA_VIDEO_CODEC_SDK_LIB_PATH "${NV_VIDEO_CODEC_SDK_DIR}/Lib/x64")
else()
  set(NVIDIA_VIDEO_CODEC_SDK_LIB_PATH "${NV_VIDEO_CODEC_SDK_DIR}/Lib/Win32")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NvVideoCodecSDK DEFAULT_MSG NV_VIDEO_CODEC_SDK_DIR)

if (NvVideoCodecSDK_FOUND)

	add_library(NvVideoCodecSDK::NvVideoCodecSDK SHARED IMPORTED)
	set_target_properties(NvVideoCodecSDK::NvVideoCodecSDK PROPERTIES IMPORTED_IMPLIB "${NVIDIA_VIDEO_CODEC_SDK_LIB_PATH}/nvencodeapi.lib")
	set_target_properties(NvVideoCodecSDK::NvVideoCodecSDK PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${NV_VIDEO_CODEC_SDK_DIR}/Interface")

endif()

unset (NVIDIA_VIDEO_CODEC_SDK_LIB_PATH)
