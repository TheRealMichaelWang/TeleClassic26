add_executable(${PROJECT_NAME}
    main.c
    server.c
    thread_pool.c
    protocol.c
    heartbeat.c
    utils.c
    log.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cNBT
)

target_link_libraries(${PROJECT_NAME} PRIVATE plibsys libcurl nbt ZLIB::ZLIB)

# Optional: set output to build/bin for cleaner layout
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
