set(PC_COM_PATH                    ${CMAKE_CURRENT_SOURCE_DIR}/../../pc_com)

# list all desired packet proto files here
set(PROTO_FILES
        MessageType.proto
        LogPrint.proto
        CLIData.proto
        LogToPlot.proto
        AddXYToPlot.proto
        AddToBodePlot.proto
        DrawPlot.proto
        DrawBodePlot.proto
        ClearPlots.proto
        ConfigPlot.proto
)


# odd that they call this cpp, but it generates C files
nanopb_generate_cpp(TARGET generated-message-c-files ${PROTO_FILES})

# copy all the build generated headers into the repo to enable
# committing these files when finalized.
add_custom_command(TARGET generated-message-c-files POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_CURRENT_BINARY_DIR}/*.h
                           ${CMAKE_CURRENT_SOURCE_DIR}/../generated/c/
)
add_custom_command(TARGET generated-message-c-files POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_CURRENT_BINARY_DIR}/*.c
                           ${CMAKE_CURRENT_SOURCE_DIR}/../generated/c/
)

# c++ generator setup
add_library(generated-message-cpp-files OBJECT ${PROTO_FILES})
target_link_libraries(generated-message-cpp-files PUBLIC protobuf::libprotobuf)
set(PROTO_CPP_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../generated/cpp)
protobuf_generate(
        TARGET generated-message-cpp-files
        PROTOC_OUT_DIR "${PROTO_CPP_OUT_DIR}"
)

set(PROTO_PYTHON_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../generated/python)
protobuf_generate_python(
        GENERATED_PYTHON_FILES
        ${PROTO_FILES}
)

add_custom_target(generated-message-python-files ALL DEPENDS ${GENERATED_PYTHON_FILES})

# copy all the build generated python files into the repo to enable
# committing these files when finalized.
add_custom_command(TARGET generated-message-python-files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/*.py
        ${PC_COM_PATH}/messages/
)
# 'messages' target.
# configure cmake and build for this target to generate
# all C, C++ and python output.
add_custom_target(messages)
add_dependencies(messages
        generated-message-c-files
        generated-message-cpp-files
        generated-message-python-files
)
