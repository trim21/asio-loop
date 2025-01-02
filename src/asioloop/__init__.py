import asyncio

from .__asioloop import EventLoop


class AsioEventLoopPolicy(asyncio.events.BaseDefaultEventLoopPolicy):

    _loop_factory = EventLoop

    def set_event_loop(self, loop: EventLoop) -> None:
        """Set the event loop."""
        self._local._set_called = True
        if loop is not None and not isinstance(loop, EventLoop):
            raise TypeError(
                f"loop must be an instance of EventLoop or None, not '{type(loop).__name__}'"
            )
        self._local._loop = loop


__all__ = ["EventLoop", "AsioEventLoopPolicy"]
