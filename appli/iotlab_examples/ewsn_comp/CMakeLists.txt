if (PLATFORM STREQUAL "iotlab-a8-m3")
    include_directories(
        ${PROJECT_SOURCE_DIR}/drivers/stm32f1xx
        ${PROJECT_SOURCE_DIR}/drivers/cortex-m3
        ${PROJECT_SOURCE_DIR}/drivers/stm32
        ${PROJECT_SOURCE_DIR}/drivers/

        ${PROJECT_SOURCE_DIR}/appli/iotlab/lib
        ${PROJECT_SOURCE_DIR}/appli/iotlab/periph
        )
    add_executable(ewsn_receiver receiver zep_sniffer gps_synced_clock)
    add_executable(ewsn_sender sender gps_synced_clock)
    target_link_libraries(ewsn_receiver
        iotlab_i2c
        zep_sniffer_format
        platform
    )
    target_link_libraries(ewsn_sender
        iotlab_i2c
        iotlab_uid_num_table
        mac_csma
        platform
    )
endif (PLATFORM STREQUAL "iotlab-a8-m3")
