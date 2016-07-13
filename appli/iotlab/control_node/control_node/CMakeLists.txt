include_directories(
    ${PROJECT_SOURCE_DIR}/drivers/stm32f1xx
    ${PROJECT_SOURCE_DIR}/drivers/cortex-m3
    ${PROJECT_SOURCE_DIR}/drivers/stm32
    ${PROJECT_SOURCE_DIR}/drivers/

    ${PROJECT_SOURCE_DIR}/appli/iotlab/periph
    ${PROJECT_SOURCE_DIR}/appli/iotlab/autotest_common
    ${PROJECT_SOURCE_DIR}/appli/iotlab/lib

    ${PROJECT_SOURCE_DIR}/lib/event
    )

if(${PLATFORM_HAS_INA226})
    add_executable(control_node
        main
        cn_meas_pkt
        cn_priority
        cn_alim
        cn_control
        cn_consumption
        cn_radio
        cn_i2c
        cn_autotest
        cn_logger
        cn_event
        )
    target_link_libraries(control_node
        platform
        fiteco_lib_gwt
        iotlab_autotest_cn
        iotlab_i2c_slave
        iotlab_serial
        iotlab_time
        iotlab_gpio
        iotlab_leds_util
        zep_sniffer_format
        )
# add_subdirectory(test_cn)
endif(${PLATFORM_HAS_INA226})

if (PLATFORM STREQUAL "iotlab-m3")
    add_executable(control_node_m3
        main
        cn_meas_pkt
        cn_priority
        cn_control
        cn_radio
        cn_logger
        cn_event
        )
target_link_libraries(control_node_m3
        platform
        iotlab_serial
        iotlab_time
        iotlab_gpio
        iotlab_leds_util
        zep_sniffer_format
    )
endif (PLATFORM STREQUAL "iotlab-m3")

add_subdirectory(tests)
