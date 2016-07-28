include_directories(${PROJECT_SOURCE_DIR}/appli/iotlab/lib)

add_executable(test_iotlab_time test_iotlab_time)
target_link_libraries(test_iotlab_time platform)
