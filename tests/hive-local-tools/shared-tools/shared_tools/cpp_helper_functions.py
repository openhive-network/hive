import os
import glob

def cpp_clear():
    for file_pattern in (
        "*.so",
        "internal_wrapper.cpp",
        ):
        for file in glob.glob(file_pattern):
            os.remove(file)

def cpp_prepare():
    cpp_source_file_name    = "cpp_functions"
    cpp_wrapper_file_name   = "internal_wrapper.cpp"
    cpp_interface_name      = "cpp_interface"

    os.system( f"g++ -shared -std=c++17 -fPIC {cpp_source_file_name}.cpp -o lib{cpp_source_file_name}.so " )

    os.system(f"cython --cplus -3 {cpp_interface_name}.pyx -o {cpp_wrapper_file_name}")

    os.system(
        "g++ -shared -std=c++17 -fPIC "
        "`python3 -m pybind11 --includes` "
        "-I . "
        f"{cpp_wrapper_file_name} "
        f"-o {cpp_interface_name}`python3-config --extension-suffix` "
        f"-L. -l{cpp_source_file_name} -Wl,-rpath,."
    )