# The generated FastDDS IDL support sources, in ONE place.
#
# All of them are unconditional for every consumer: media_transport.cpp's
# WriterCore::init/make_type_support reference every FrameKind PubSubType, so even a
# subscriber-only consumer needs the whole set. (With a STATIC archive the linker still
# only pulls the objects actually referenced.)
#
# include()d by common/CMakeLists.txt (the ai_common::media target) and by
# common/media_transport/CMakeLists.txt (the standalone media_bench project), so adding a
# 9th FrameKind is a one-line change that cannot leave the bench behind.

set(AI_COMMON_IDL_DIR ${CMAKE_CURRENT_LIST_DIR}/generated/idl)

set(AI_COMMON_IDL_SOURCES
  ${AI_COMMON_IDL_DIR}/image_framePubSubTypes.cxx
  ${AI_COMMON_IDL_DIR}/image_frameTypeObjectSupport.cxx
  ${AI_COMMON_IDL_DIR}/image360_framePubSubTypes.cxx
  ${AI_COMMON_IDL_DIR}/image360_frameTypeObjectSupport.cxx
  ${AI_COMMON_IDL_DIR}/lidar_framePubSubTypes.cxx
  ${AI_COMMON_IDL_DIR}/lidar_frameTypeObjectSupport.cxx
  ${AI_COMMON_IDL_DIR}/imu_framePubSubTypes.cxx
  ${AI_COMMON_IDL_DIR}/imu_frameTypeObjectSupport.cxx
)
