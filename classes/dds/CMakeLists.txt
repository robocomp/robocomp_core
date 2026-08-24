# Librería DDS mínima y standalone, reutilizable desde cualquier componente
# RoboComp vía add_subdirectory($ENV{ROBOCOMP}/classes/dds ...) +
# target_link_libraries(mi_target PRIVATE dds).
#
# Copiada desde experimental/dds_minimal_examples (mismo repo, robocomp-robolab),
# donde viven los binarios de demo (vector_publisher_example/vector_subscriber_example)
# y la documentación completa de la arquitectura. Esta copia NO se sincroniza
# sola: si el original cambia, hay que volver a copiar a mano.
#
# Layout: core/ = el contrato (DDSChannel/DDSOptional/DDSSubscription/Handle,
# sin FastDDS, se incluye siempre); null_backend/ y sensors/{lidar,camera,imu,
# image360}/+media_transport/ = las dos implementaciones intercambiables de
# ese contrato, elegidas en tiempo de configuración según DDS_BACKEND.
cmake_minimum_required(VERSION 3.16)
project(dds LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# DDS_BACKEND: AUTO (por defecto) detecta FastDDS con find_package(...QUIET) y
# cae al backend nulo si no está, sin error. ON lo EXIGE (find_package(...REQUIRED):
# error de configuración si falta, en vez de caer al stub en silencio). OFF fuerza
# el backend nulo aunque FastDDS esté instalado -- útil para probar deliberadamente
# la rama "sin DDS" en una máquina que sí lo tiene.
if (NOT DEFINED DDS_BACKEND)
  set(DDS_BACKEND "AUTO" CACHE STRING "Backend DDS: AUTO detecta FastDDS (fallback silencioso a nulo), ON lo exige, OFF fuerza el backend nulo")
  set_property(CACHE DDS_BACKEND PROPERTY STRINGS AUTO ON OFF)
endif()

if (DDS_BACKEND STREQUAL "ON")
  find_package(fastcdr REQUIRED)
  find_package(fastdds REQUIRED)
elseif (NOT DDS_BACKEND STREQUAL "OFF")
  find_package(fastcdr QUIET)
  find_package(fastdds QUIET)
endif()

if (fastcdr_FOUND AND fastdds_FOUND AND NOT DDS_BACKEND STREQUAL "OFF")
  message(STATUS "classes/dds: FastDDS encontrado -> canales reales")

  set(MEDIA_TRANSPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/media_transport)
  include(${MEDIA_TRANSPORT_DIR}/idl_sources.cmake)

  if (NOT TARGET media_transport_min)
    add_library(media_transport_min STATIC
      ${MEDIA_TRANSPORT_DIR}/media_transport.cpp
      ${AI_COMMON_IDL_SOURCES}
    )
    target_include_directories(media_transport_min PUBLIC ${MEDIA_TRANSPORT_DIR})
    target_link_libraries(media_transport_min PUBLIC fastdds fastcdr)
  endif()

  set(DDS_CHANNEL_SOURCES
    sensors/lidar/lidar_channel.cpp
    sensors/lidar/lidar_subscription.cpp
    sensors/camera/camera_channel.cpp
    sensors/camera/camera_subscription.cpp
    sensors/imu/imu_channel.cpp
    sensors/imu/imu_subscription.cpp
    sensors/image360/image360_channel.cpp
    sensors/image360/image360_subscription.cpp
  )
else()
  if (DDS_BACKEND STREQUAL "OFF")
    message(STATUS "classes/dds: DDS_BACKEND=OFF -> canales nulos forzados (FastDDS ni se ha buscado)")
  else()
    message(WARNING "classes/dds: FastDDS NO encontrado -> canales nulos (no-op)")
  endif()

  set(DDS_CHANNEL_SOURCES null_backend/dds_stubs.cpp)
endif()

add_library(dds STATIC ${DDS_CHANNEL_SOURCES})
target_include_directories(dds PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if (TARGET media_transport_min)
  target_link_libraries(dds PUBLIC media_transport_min)
endif()
