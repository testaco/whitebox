add_executable(gr_udp_sink gr_udp_sink.c)
target_link_libraries(gr_udp_sink ${CMAKE_REQUIRED_LIBRARIES} whitebox)

install(TARGETS
    gr_udp_sink
	RUNTIME DESTINATION bin
)
