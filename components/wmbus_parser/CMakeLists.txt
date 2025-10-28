# Allow building C++ sources included in this custom component
set(COMPONENT_SRCS "wmbus_parser.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ".")

# Drivers
set(COMPONENT_SRCS "${COMPONENT_SRCS}" "drivers/evo868_driver.cpp")
set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}" "drivers")

register_component()
