set(PYTHON_TESTS_COMMAND_ARGS -m unittest discover ${CMAKE_CURRENT_SOURCE_DIR})
set(PYTHON_TESTS test_py_opendaq)

set(PYHTON_TEST_SRC
    opendaq_test.py
    test_architecture.py
    test_opendaq_device.py
    test_opendaq_instance.py
    test_core_types.py
    test_core_type_integer.py
    test_core_type_string.py
    test_doc_quick_start.py
    test_how_to.py
    test_how_to_add_function_block.py
    test_reader_datetime.py
    test_property_system.py
    test_how_to_last_value.py
    test_freezable.py
    test_type_manager.py
)

# Test-only module, imported by the tests from the module's output directory
pybind11_add_module(opendaq_test_helpers
    MODULE
        helpers/opendaq_test_helpers.cpp
)

target_link_libraries(opendaq_test_helpers
    PRIVATE
        daq::py_core_types
)

set_target_properties(opendaq_test_helpers
    PROPERTIES
        DEBUG_POSTFIX ""
)

if(UNIX AND NOT APPLE)
    set_target_properties(opendaq_test_helpers PROPERTIES
        BUILD_RPATH_USE_ORIGIN TRUE
    )
endif()

option(OPENDAQ_VENV_PYTHON_EXECUTABLE_TESTS "Use python executable from virtual environment for tests" OFF)
set(PYTHON_EXECUTABLE ${Python_EXECUTABLE})

if(OPENDAQ_VENV_PYTHON_EXECUTABLE_TESTS)
    set(PYTHON_EXECUTABLE python)
endif()

message(STATUS "Python executable for tests: ${PYTHON_EXECUTABLE}")

add_custom_target(
    ${PYTHON_TESTS}
    COMMAND ${PYTHON_EXECUTABLE} ${PYTHON_TESTS_COMMAND_ARGS}
    USES_TERMINAL
    COMMENT "Running python bindings tests"
    SOURCES ${PYHTON_TEST_SRC}
    WORKING_DIRECTORY $<TARGET_FILE_DIR:py_opendaq_daq>
)

list(JOIN PYTHON_TESTS_COMMAND_ARGS " " PYTHON_TESTS_COMMAND_ARGS_STRING)

set_target_properties(
    ${PYTHON_TESTS}
    PROPERTIES
        VS_DEBUGGER_COMMAND ${PYTHON_EXECUTABLE}
        VS_DEBUGGER_COMMAND_ARGUMENTS ${PYTHON_TESTS_COMMAND_ARGS_STRING}
        VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:py_opendaq_daq>
)

add_test(
    NAME ${PYTHON_TESTS}
    COMMAND ${PYTHON_EXECUTABLE} -m unittest discover ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY $<TARGET_FILE_DIR:py_opendaq_daq>
)

set_tests_properties(test_py_opendaq PROPERTIES LABELS "python_bindings")
# The python tests start the standard servers on the default ports
opendaq_test_resource_lock(${PYTHON_TESTS} network_native network_lt network_opcua mdns)
