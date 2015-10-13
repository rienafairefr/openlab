if(${PLATFORM_HAS_CSMA})
add_executable(distributed_algorithm
    main computing radio_network
    poisson_clock
    clock_convergence
    )
target_link_libraries(distributed_algorithm shell platform mac_csma)
endif(${PLATFORM_HAS_CSMA})
