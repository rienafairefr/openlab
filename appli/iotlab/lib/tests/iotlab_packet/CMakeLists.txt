include_directories(${PROJECT_SOURCE_DIR}/appli/iotlab/lib)

add_executable(test_iotlab_packet test_iotlab_packet)
target_link_libraries(test_iotlab_packet iotlab_packet platform)
