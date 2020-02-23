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

add_executable(i2c_clock
    main
    cn_meas_pkt
    cn_priority
    cn_control
    cn_radio
    cn_logger
    cn_event
    )
target_link_libraries(i2c_clock
    platform
    iotlab_serial
    iotlab_time
    iotlab_gpio
    iotlab_leds_util
    zep_sniffer_format
)
