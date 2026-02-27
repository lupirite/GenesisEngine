project(GenesisEditor)

# Define the library
add_library(GenesisEditor INTERFACE)

# Tell CMake where the headers are
target_include_directories(GenesisEditor INTERFACE include)

# --- THE AUTO-REGISTER MAGIC ---
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GenesisEditor)