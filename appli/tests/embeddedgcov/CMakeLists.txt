add_executable(test_embeddedgcov test_embeddedgcov max_int)
target_link_libraries(test_embeddedgcov embeddedgcov platform)

set_source_files_properties(max_int test_embeddedgcov
	PROPERTIES COMPILE_FLAGS "-fprofile-arcs -ftest-coverage"
)
set_target_properties(test_embeddedgcov
	PROPERTIES LINK_FLAGS "-lgcov -fprofile-arcs -ftest-coverage"
)
