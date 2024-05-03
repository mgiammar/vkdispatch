import vkdispatch as vd

import copy
import numpy as np

class push_constant_buffer:
    def __init__(self, pc_dict: dict) -> None:
        self.pc_dict: dict[str, tuple[int, vd.dtype]] =  copy.deepcopy(pc_dict)
        self.ref_dict: dict[str, int] = {}
        self.pc_list: list[np.ndarray] = [None] * len(self.pc_dict)
        self.var_types: list[vd.dtype] = [None] * len(self.pc_dict)
        self.numpy_dtypes: list = [None] * len(self.pc_dict)
        self.size = 0

        for key, val in self.pc_dict.items():
            ii, var_type = val

            self.ref_dict[key] = ii
            
            dtype = vd.to_numpy_dtype(var_type.scalar)
            self.numpy_dtypes[ii] = dtype
            self.pc_list[ii] = np.zeros(shape=var_type.numpy_shape, dtype=self.numpy_dtypes[ii])
            self.var_types[ii] = var_type

            self.size += var_type.item_size
    
    def __setitem__(self, key: str, value: np.ndarray | list | tuple | int | float) -> None:
        if key not in self.ref_dict:
            raise ValueError(f"Invalid push constant '{key}'!")
        
        ii = self.ref_dict[key]

        if not isinstance(value, np.ndarray) and not isinstance(value, list) and not isinstance(value, tuple):
            self.pc_list[ii][0] = value
            return
        
        arr = np.array(value, dtype=self.numpy_dtypes[ii])

        if arr.shape != self.var_types[ii].numpy_shape:
            raise ValueError(f"The shape of {key} is {self.var_types[ii].numpy_shape} but {arr.shape} was given!")

        self.pc_list[ii] = arr
    
    def get_bytes(self):
        return b''.join([elem.tobytes() for elem in self.pc_list])

class shader_builder:
    def __init__(self) -> None:
        self.var_count = 0
        self.binding_count = 0
        self.pc_dict: dict[str, tuple[int, vd.dtype]] = {}
        self.pc_list: list[tuple[str, vd.dtype, str]] = []
        self.pc_size = 0
        self.scope_num = 1
        self.global_x = self.make_var(vd.uint32, "gl_GlobalInvocationID.x")
        self.global_y = self.make_var(vd.uint32, "gl_GlobalInvocationID.y")
        self.global_z = self.make_var(vd.uint32, "gl_GlobalInvocationID.z")
        self.contents = ""

        self.header  = "#version 450\n"
        self.header += "#extension GL_ARB_separate_shader_objects : enable\n"
        self.header += "#extension GL_EXT_debug_printf : enable\n"
        self.header += "#extension GL_EXT_shader_atomic_float : enable\n"
    
    def reset(self) -> None:
        self.var_count = 0
        self.binding_count = 0
        self.pc_dict = {}
        self.pc_list = []
        self.pc_size = 0
        self.scope_num = 1
        self.global_x = self.make_var(vd.uint32, "gl_GlobalInvocationID.x")
        self.global_y = self.make_var(vd.uint32, "gl_GlobalInvocationID.y")
        self.global_z = self.make_var(vd.uint32, "gl_GlobalInvocationID.z")
        self.contents = ""

        self.header  = "#version 450\n"
        self.header += "#extension GL_ARB_separate_shader_objects : enable\n"
        self.header += "#extension GL_EXT_debug_printf : enable\n"
        self.header += "#extension GL_EXT_shader_atomic_float : enable\n"

    def get_name(self, var_name: str = None) -> str:
        new_var = f"var{self.var_count}" if var_name is None else var_name
        if var_name is None:
            self.var_count += 1
        return new_var

    def make_var(self, var_type: vd.dtype, var_name: str = None) -> vd.shader_variable:
        return vd.shader_variable(self.append_contents, self.get_name, var_type, var_name)
    
    def push_constant(self, var_type: vd.dtype, var_name: str) -> None:
        new_var = self.make_var(var_type, f"PC.{var_name}")
        self.pc_list.append((var_name, var_type, f"{var_type.glsl_type} {var_name};"))
        self.pc_size += var_type.item_size
        return new_var
    
    def new(self, var_type: vd.dtype, var_name: str = None) -> vd.shader_variable:
        new_var = self.make_var(var_type, var_name)
        self.append_contents(f"{var_type.glsl_type} {new_var};\n")
        return new_var

    def buffer(self, var_type: vd.dtype, var_name: str = None) -> vd.shader_variable:
        buffer_name = f"buf{self.binding_count}" if var_name is None else var_name
        new_var = self.make_var(var_type, f"{buffer_name}.data")
        self.header += f"layout(set = 0, binding = {self.binding_count}) buffer Buffer{self.binding_count} {{ {var_type.glsl_type} data[]; }} {buffer_name};\n"
        new_var.binding = self.binding_count
        self.binding_count += 1
        return new_var
    
    def if_statement(self, arg: vd.shader_variable) -> None:
        self.append_contents(f"if({arg}) {'{'}\n")
        self.scope_num += 1

    def if_any(self, *args: list[vd.shader_variable]) -> None:
        self.append_contents(f"if({' || '.join([str(elem) for elem in args])}) {'{'}\n")
        self.scope_num += 1

    def if_all(self, *args: list[vd.shader_variable]) -> None:
        self.append_contents(f"if({' && '.join([str(elem) for elem in args])}) {'{'}\n")
        self.scope_num += 1
    
    def else_statement(self) -> None:
        self.append_contents("} else {'\n")

    def return_statement(self, arg = None) -> None:
        arg = arg if arg is not None else ""
        self.append_contents(f"return {arg};\n")
    
    def end_if(self) -> None:
        self.scope_num -= 1        
        self.append_contents("}\n")
    
    def ceil(self, arg: vd.shader_variable) -> vd.shader_variable:
        return self.make_var(arg.var_type, f"ceil({arg})")

    def exp(self, arg: vd.shader_variable) -> vd.shader_variable:
        return self.make_var(arg.var_type, f"exp({arg})")
    
    def sin(self, arg: vd.shader_variable) -> vd.shader_variable:
        return self.make_var(arg.var_type, f"sin({arg})")

    def cos(self, arg: vd.shader_variable) -> vd.shader_variable:
        return self.make_var(arg.var_type, f"cos({arg})")
    
    def atomic_add(self, arg1: vd.shader_variable, arg2: vd.shader_variable) -> vd.shader_variable:
        new_var = self.new(arg1.var_type)
        self.append_contents(f"{new_var} = atomicAdd({arg1}, {arg2});\n")
        return new_var

    def float_bits_to_int(self, arg: vd.shader_variable) -> vd.shader_variable:
        return self.make_var(vd.int32, f"floatBitsToInt({arg})")
    
    def int_bits_to_float(self, arg: vd.shader_variable) -> vd.shader_variable:
        return self.make_var(vd.float32, f"intBitsToFloat({arg})")

    def print(self, *args: vd.shader_variable | str) -> None:
        args_list = []

        fmt = ""

        for arg in args:
            if isinstance(arg, vd.shader_variable):
                args_list.append(arg.printf_args())
                fmt += arg.format
            else:
                fmt += arg.__repr__()


        self.append_contents(f'debugPrintfEXT("{fmt}", {",".join(args_list)});\n')

    def append_contents(self, contents: str) -> None:
        self.contents += ("\t" * self.scope_num) + contents

    def build(self, x: int, y: int, z: int) -> str:
        self.pc_list.sort(key=lambda x: x[1].item_size, reverse=True)
        self.pc_dict = {elem[0]: (ii, elem[1]) for ii, elem in enumerate(self.pc_list)}

        if self.pc_list:
            push_constant_contents = "\n".join([f"\t{elem[2]}" for elem in self.pc_list])

            self.header += f"\nlayout(push_constant) uniform PushConstant {{\n { push_constant_contents } \n}} PC;\n"

        return self.header + f"\nlayout(local_size_x = {x}, local_size_y = {y}, local_size_z = {z}) in;\nvoid main() {'{'}\n" + self.contents + "\n}"
    
shader = shader_builder()