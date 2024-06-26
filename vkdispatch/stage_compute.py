from typing import Tuple

import vkdispatch as vd
import vkdispatch_native
from .command_list import CommandList
from .descriptor_set import DescriptorSet


class ComputePlan:
    def __init__(self, shader_source: str, binding_count: int, pc_size: int) -> None:

        self.binding_count = binding_count
        self.pc_size = pc_size
        self.shader_source = shader_source

        # for ii, line in enumerate(shader_source.split("\n")):
        #    print(f"{ii + 1:03d} | {line}")

        self._handle = vkdispatch_native.stage_compute_plan_create(
            vd.get_context_handle(), shader_source.encode(), binding_count, pc_size
        )

    def record(
        self,
        command_list: CommandList,
        descriptor_set: DescriptorSet,
        blocks: Tuple[int, int, int],
    ) -> None:
        vkdispatch_native.stage_compute_record(
            command_list._handle,
            self._handle,
            descriptor_set._handle,
            blocks[0],
            blocks[1],
            blocks[2],
        )
