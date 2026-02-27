project(GenesisMath)

# Define the library
add_library(GenesisMath INTERFACE)

# Tell CMake where the headers are
target_include_directories(GenesisMath INTERFACE include)

# --- THE AUTO-REGISTER MAGIC ---
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GenesisMath)