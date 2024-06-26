import vkdispatch as vd
import vkdispatch_native


class DescriptorSet:
    """TODO: Docstring"""

    _handle: int

    def __init__(self, compute_plan_handle: int) -> None:
        self._handle = vkdispatch_native.descriptor_set_create(compute_plan_handle)

    def bind_buffer(self, buffer: vd.Buffer, binding: int) -> None:
        vkdispatch_native.descriptor_set_write_buffer(
            self._handle, binding, buffer._handle
        )
