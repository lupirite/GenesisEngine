import os
import re

def generate_cpp(math_code):
    # Convert GLSL style to our Genesis namespace
    code = math_code.replace("vec3", "Genesis::vec3")
    #code = math_code.replace("float", "double") # High precision on CPU

    output = """#include "Vector.hpp"
namespace Genesis {
""" + code + """
}"""
    return output

def generate_glsl(math_code):
    code = "#version 450\n" + math_code

    return code


# This is a very basic starter for your cross-compiler
def transpile(input_file, output_cpp, output_glsl):
    with open(input_file, 'r') as f:
        code = f.read()

    # 1. Generate C++ version
    # We replace 'vec3' with our custom 'Engine::vec3' and 'float' with 'double'
    cpp_version = generate_cpp(code)
    with open(output_cpp, 'w') as f:
        f.write("// GENERATED CODE - DO NOT EDIT\n")
        f.write(cpp_version)

    # 2. Generate GLSL version
    # We keep 'vec3' and 'float' but add Vulkan-specific headers
    glsl_version = generate_glsl(code)
    with open(output_glsl, 'w') as f:
        f.write("// GENERATED CODE - DO NOT EDIT\n")
        f.write(glsl_version)

    print(f"Successfully transpiled {input_file}")

# Example usage (we can automate this with CMake later)
if __name__ == "__main__":
    # Get the directory where transpiler.py is located (GenesisND/scripts)
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Move up one level to the project root (GenesisND)
    project_root = os.path.dirname(script_dir)

    # Change the current working directory to the project root
    os.chdir(project_root)

    # Now these paths will always work, regardless of where you launch from!
    os.makedirs("generated/cpp", exist_ok=True)
    os.makedirs("generated/shaders", exist_ok=True)

    for filename in os.listdir("math_src"):
        if filename.endswith(".math"):
            name = filename.replace(".math", "")
            transpile(f"math_src/{filename}",
                      f"generated/cpp/{name}.hpp",
                      f"generated/shaders/{name}.glsl")