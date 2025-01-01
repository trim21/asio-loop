import asyncio
import sys
import threading
from .__asioloop import EventLoop


class AsioEventLoopPolicy(asyncio.AbstractEventLoopPolicy):

    _loop_factory = EventLoop

    class _Local(threading.local):
        _loop = None
        _set_called = False

    def __init__(self):
        self._local = self._Local()

    def get_event_loop(self):
        """Get the event loop for the current context.

        Returns an instance of EventLoop or raises an exception.
        """
        if (
            self._local._loop is None
            and not self._local._set_called
            and threading.current_thread() is threading.main_thread()
        ):
            stacklevel = 2
            f = sys._getframe(1)
            # Move up the call stack so that the warning is attached
            # to the line outside asyncio itself.
            while f:
                module = f.f_globals.get("__name__")
                if not (module == "asyncio" or module.startswith("asyncio.")):
                    break
                f = f.f_back
                stacklevel += 1
            import warnings

            warnings.warn(
                "There is no current event loop",
                DeprecationWarning,
                stacklevel=stacklevel,
            )
            self.set_event_loop(self.new_event_loop())

        if self._local._loop is None:
            raise RuntimeError(
                "There is no current event loop in thread %r."
                % threading.current_thread().name
            )

        return self._local._loop

    def set_event_loop(self, loop):
        """Set the event loop."""
        self._local._set_called = True
        if loop is not None and not isinstance(loop, EventLoop):
            raise TypeError(
                f"loop must be an instance of EventLoop or None, not '{type(loop).__name__}'"
            )
        self._local._loop = loop

    def new_event_loop(self):
        """Create a new event loop.

        You must call set_event_loop() to make this the current event
        loop.
        """
        return self._loop_factory()


# print(A.__bases__)
# EventLoop.__bases__ = (asyncio.AbstractEventLoop,)

__all__ = ["EventLoop", "AsioEventLoopPolicy"]
