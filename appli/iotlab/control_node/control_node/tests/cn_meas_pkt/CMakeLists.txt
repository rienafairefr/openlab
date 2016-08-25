include_directories(
    ${PROJECT_SOURCE_DIR}/appli/iotlab/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
)

add_executable(test_cn_meas_pkt test_cn_meas_pkt)
target_link_libraries(test_cn_meas_pkt iotlab_serial platform)
