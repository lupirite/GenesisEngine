project(ModuleTemplate)

# Define the library
add_library(ModuleTemplate INTERFACE)

target_include_directories(ModuleTemplate PUBLIC include)

# --- THE AUTO-REGISTER MAGIC ---
# Add this target's name to the global list
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST ModuleTemplate)