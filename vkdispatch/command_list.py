from typing import Any
from typing import Callable
from typing import List

import numpy as np

import vkdispatch as vd
import vkdispatch_native


class CommandList:
    """TODO: Docstring"""

    _handle: int
    _reset_on_submit: bool
    pc_buffers: List
    descriptor_sets: List

    def __init__(self, reset_on_submit: bool = False) -> None:
        self._handle = vkdispatch_native.command_list_create(vd.get_context_handle())
        self.pc_buffers = []
        self.descriptor_sets = []
        self._reset_on_submit = reset_on_submit

    def __del__(self) -> None:
        pass  # vkdispatch_native.command_list_destroy(self._handle)

    def get_instance_size(self) -> int:
        """Get the total size of the command list in bytes."""
        return vkdispatch_native.command_list_get_instance_size(self._handle)

    def add_pc_buffer(self, pc_buffer: "vd.push_constant_buffer") -> None:
        """Add a push constant buffer to the command list."""
        self.pc_buffers.append(pc_buffer)

    def add_desctiptor_set(self, descriptor_set: "vd.descriptor_set") -> None:
        """Add a descriptor set to the command list."""
        self.descriptor_sets.append(descriptor_set)

    def reset(self) -> None:
        """Reset the command list by clearing the push constant buffer and descriptor
        set lists. The call to command_list_reset frees all associated memory.
        """
        self.pc_buffers = []
        self.descriptor_sets = []
        vkdispatch_native.command_list_reset(self._handle)

    def submit(self, device_index: int = 0, data: bytes = None) -> None:
        """Submit the command list to the specified device with additional data to
        append to the front of the command list.
        
        Parameters:
        device_index (int): The device index to submit the command list to.\
                Default is 0.
        data (bytes): The additional data to append to the front of the command list.
        """
        instances = None

        if data is None:
            data = b""

            for pc_buffer in self.pc_buffers:
                data += pc_buffer.get_bytes()

            instances = 1

            if len(data) != self.get_instance_size():
                raise ValueError("Push constant buffer size mismatch!")
        else:
            if len(data) % self.get_instance_size() != 0:
                raise ValueError("Push constant buffer size mismatch!")

            instances = len(data) // self.get_instance_size()

        vkdispatch_native.command_list_submit(
            self._handle, data, instances, device_index
        )

        if self._reset_on_submit:
            self.reset()


__cmd_list = None


def get_command_list() -> CommandList:
    global __cmd_list

    if __cmd_list is None:
        __cmd_list = CommandList(reset_on_submit=True)

    return __cmd_list


def get_command_list_handle() -> int:
    return get_command_list()._handle
