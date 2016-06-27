include_directories(${PROJECT_SOURCE_DIR}/appli/iotlab/periph)

if (PLATFORM STREQUAL "iotlab-m3")
add_executable(embers_sensors main)
target_link_libraries(embers_sensors
    shell
    lps331ap
    isl29020
    platform
    )
endif (PLATFORM STREQUAL "iotlab-m3")
