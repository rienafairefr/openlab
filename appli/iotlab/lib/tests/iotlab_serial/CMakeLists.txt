include_directories(${PROJECT_SOURCE_DIR}/appli/iotlab/lib)

add_executable(test_iotlab_serial test_iotlab_serial)
target_link_libraries(test_iotlab_serial iotlab_packet platform)
