include_directories(${PROJECT_SOURCE_DIR}/appli/iotlab/lib)

# Build for open nodes

add_executable(radio_energy_measurement main)
target_link_libraries(radio_energy_measurement shell platform)
