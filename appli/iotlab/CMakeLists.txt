# Add the tasks directory
# iotlab specific code

if (PLATFORM MATCHES "iotlab-.*")

add_subdirectory(control_node)
add_subdirectory(idle_with_dc)


add_subdirectory(node_radio_characterization)
add_subdirectory(radio_test)

# Libraries
add_subdirectory(autotest_common)
add_subdirectory(lib)
add_subdirectory(periph)

# Add the fiteco directory
add_subdirectory(fiteco_test)

add_subdirectory(radio_energy_measurement)

add_subdirectory(open_node_m3_autotest)
endif (PLATFORM MATCHES "iotlab-.*")

if (PLATFORM STREQUAL "agile-fox")
add_subdirectory(open_node_m3_autotest)
endif (PLATFORM STREQUAL "agile-fox")
