cmake_minimum_required(VERSION 3.14)
project(edge-ai-manager VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization Flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -Wall -Wextra)
else()
    add_compile_options(-O3 -Wall -Wextra)
endif()

# Build Options
option(ENABLE_DEEPSTREAM  "Enable NVIDIA DeepStream SDK multi-camera acceleration" OFF)
option(ENABLE_JETSON_CUDA "Enable CUDA & TensorRT hardware acceleration" OFF)
option(ENABLE_PAHO_MQTT   "Link Eclipse Paho MQTT C client" OFF)
option(ENABLE_SQLITE3     "Link SQLite3 database engine" OFF)
option(ENABLE_YAML_CPP    "Link yaml-cpp parser library" OFF)
option(BUILD_TESTS        "Build unit tests" OFF)

# Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Core Sources (App, Service, Mock HAL, Jetson HAL)
set(SOURCES
    # App Layer
    src/app/main.cpp
    src/app/edge_ai_manager.cpp
    src/app/pipeline.cpp
    src/app/rule_engine.cpp
    src/app/rules/zone_rule.cpp
    src/app/rules/counting_rule.cpp

    # Service Layer
    src/service/event_bus.cpp
    src/service/preprocessor.cpp
    src/service/postprocessor.cpp
    src/service/camera_service.cpp
    src/service/inference_service.cpp
    src/service/kalman_filter.cpp
    src/service/hungarian.cpp
    src/service/tracker_service.cpp
    src/service/mqtt_service.cpp
    src/service/storage_service.cpp
    src/service/health_service.cpp
    src/service/config_service.cpp
    src/service/log_service.cpp

    # Jetson HAL
    src/hal/jetson/jetson_deepstream_hal.cpp
    src/hal/jetson/jetson_camera_hal.cpp
    src/hal/jetson/jetson_gpu_hal.cpp
    src/hal/jetson/jetson_network_hal.cpp
    src/hal/jetson/jetson_storage_hal.cpp
    src/hal/jetson/jetson_sensor_hal.cpp
    src/hal/jetson/jetson_watchdog_hal.cpp
)

# OpenCV Dependency
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Conditional Dependencies
set(EXTRA_LIBS "")

if(ENABLE_DEEPSTREAM)
    add_definitions(-DENABLE_DEEPSTREAM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0)
    include_directories(${GST_INCLUDE_DIRS})
    list(APPEND EXTRA_LIBS ${GST_LIBRARIES})

    find_path(DEEPSTREAM_INCLUDE_DIR nvds_version.h
        PATHS
            /opt/nvidia/deepstream/deepstream/sources/includes
            /opt/nvidia/deepstream/deepstream-6.0/sources/includes
            /opt/nvidia/deepstream/deepstream-6.3/sources/includes
            /opt/nvidia/deepstream/deepstream-5.1/sources/includes
    )
    if(DEEPSTREAM_INCLUDE_DIR)
        include_directories(${DEEPSTREAM_INCLUDE_DIR})
    endif()
    find_library(NVDS_META_LIB nvds_meta
        PATHS
            /opt/nvidia/deepstream/deepstream/lib
            /opt/nvidia/deepstream/deepstream-6.0/lib
            /opt/nvidia/deepstream/deepstream-6.3/lib
    )
    if(NVDS_META_LIB)
        list(APPEND EXTRA_LIBS ${NVDS_META_LIB})
    endif()
endif()

if(ENABLE_JETSON_CUDA)
    add_definitions(-DENABLE_JETSON_CUDA)
    find_package(CUDA REQUIRED)
    include_directories(${CUDA_INCLUDE_DIRS})
    list(APPEND EXTRA_LIBS ${CUDA_LIBRARIES} nvinfer nvonnxparser)
endif()

if(ENABLE_PAHO_MQTT)
    add_definitions(-DENABLE_PAHO_MQTT)
    list(APPEND EXTRA_LIBS paho-mqtt3a paho-mqtt3c)
endif()

if(ENABLE_SQLITE3)
    add_definitions(-DENABLE_SQLITE3)
    find_package(SQLite3 REQUIRED)
    list(APPEND EXTRA_LIBS SQLite::SQLite3)
endif()

if(ENABLE_YAML_CPP)
    add_definitions(-DENABLE_YAML_CPP)
    find_package(yaml-cpp REQUIRED)
    list(APPEND EXTRA_LIBS yaml-cpp)
endif()

# Target Executable
add_executable(edge-ai-manager ${SOURCES})
target_link_libraries(edge-ai-manager PRIVATE ${OpenCV_LIBS} ${EXTRA_LIBS})

# Posix Threads
find_package(Threads REQUIRED)
target_link_libraries(edge-ai-manager PRIVATE Threads::Threads)

# Installation Targets for Yocto Packaging
install(TARGETS edge-ai-manager DESTINATION /usr/bin)
install(DIRECTORY config/ DESTINATION /opt/jetson-ai-detect/config)
install(DIRECTORY models/ DESTINATION /opt/jetson-ai-detect/models)

# Unit Testing
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

