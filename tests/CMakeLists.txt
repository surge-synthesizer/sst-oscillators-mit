add_executable(sst-oscillators-mit-tests)
target_include_directories(sst-oscillators-mit-tests PRIVATE .)
target_link_libraries(sst-oscillators-mit-tests PRIVATE ${PROJECT_NAME})
target_sources(sst-oscillators-mit-tests
    PRIVATE
        tests.cpp
)

add_custom_command(TARGET sst-oscillators-mit-tests
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E echo "Copying $<TARGET_FILE:sst-oscillators-mit-tests> to test-binary"
        COMMAND ${CMAKE_COMMAND} -E make_directory test-binary
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:sst-oscillators-mit-tests>" test-binary)
