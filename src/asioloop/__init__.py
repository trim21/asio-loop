from __future__ import annotations

import asyncio
import asyncio.log
import sys
import threading
import warnings
import weakref
from asyncio import isfuture, tasks
from concurrent.futures import ThreadPoolExecutor
from typing import Any

from asioloop.__asioloop import EventLoop as _EventLoop

__all__ = ["EventLoop", "AsioEventLoopPolicy"]


logger = asyncio.log.logger


class EventLoop(_EventLoop):
    def __init__(self) -> None:
        super().__init__()
        self._default_executor = None
        # A weak set of all asynchronous generators that are
        # being iterated by the loop.
        self._asyncgens = weakref.WeakSet()

        # Set to True when `loop.shutdown_asyncgens` is called.
        self._asyncgens_shutdown_called = False
        # Set to True when `loop.shutdown_default_executor` is called.
        self._executor_shutdown_called = False

    @property
    def _debug(self) -> bool:
        return self.get_debug()

    def call_soon(self, callback, *args, context=None) -> None:
        return super().call_soon(callback, *args, context=context)

    async def getaddrinfo(self, host, port, *, family=0, type=0, proto=0, flags=0):
        return await super().getaddrinfo(
            host,
            port,
            family=family,
            type=type,
            proto=proto,
            flags=flags,
        )

    def set_default_executor(self, executor):
        if not isinstance(executor, ThreadPoolExecutor):
            raise TypeError("executor must be ThreadPoolExecutor instance")
        self._default_executor = executor

    async def shutdown_default_executor(self):
        """Schedule the shutdown of the default executor."""
        self._executor_shutdown_called = True
        if self._default_executor is None:
            return
        future = self.create_future()
        thread = threading.Thread(target=self._do_shutdown, args=(future,))
        thread.start()
        try:
            await future
        finally:
            thread.join()

    def _do_shutdown(self, future):
        try:
            self._default_executor.shutdown(wait=True)
            if not self.is_closed():
                self.call_soon_threadsafe(future.set_result, None)
        except Exception as ex:
            if not self.is_closed():
                self.call_soon_threadsafe(future.set_exception, ex)

    def run_in_executor(self, executor, func, *args):
        if executor is None:
            executor = self._default_executor
            # Only check when the default executor is being used
            if executor is None:
                executor = ThreadPoolExecutor(thread_name_prefix="asyncio")
                self._default_executor = executor

        return asyncio.futures.wrap_future(executor.submit(func, *args), loop=self)

    def run_forever(self) -> None:
        old_agen_hooks = sys.get_asyncgen_hooks()

        sys.set_asyncgen_hooks(
            firstiter=self._asyncgen_firstiter_hook,
            finalizer=self._asyncgen_finalizer_hook,
        )
        try:
            return super().run_forever()
        finally:
            sys.set_asyncgen_hooks(*old_agen_hooks)

    def run_until_complete(self, future: Any):
        """Run until the Future is done.

        If the argument is a coroutine, it is wrapped in a Task.

        WARNING: It would be disastrous to call run_until_complete()
        with the same coroutine twice -- it would wrap it in two
        different Tasks and that can't be good.

        Return the Future's result, or raise its exception.
        """
        new_task = not isfuture(future)
        future = asyncio.ensure_future(future, loop=self)
        if new_task:
            # An exception is raised if the future didn't complete, so there
            # is no need to log the "destroy pending task" message
            future._log_destroy_pending = False

        def done_cb(fut):
            if not fut.cancelled():
                exc = fut.exception()
                if isinstance(exc, (SystemExit, KeyboardInterrupt)):
                    # Issue #336: run_forever() already finished,
                    # no need to stop it.
                    return
            self.stop()

        future.add_done_callback(done_cb)
        try:
            self.run_forever()
        except BaseException:
            if new_task and future.done() and not future.cancelled():
                # The coroutine raised a BaseException. Consume the exception
                # to not log a warning, the caller doesn't have access to the
                # local task.
                future.exception()
            raise
        finally:
            future.remove_done_callback(done_cb)
        if not future.done():
            raise RuntimeError("Event loop stopped before Future completed.")

        return future.result()

    def _asyncgen_finalizer_hook(self, agen):
        self._asyncgens.discard(agen)
        if not self.is_closed():
            self.call_soon_threadsafe(self.create_task, agen.aclose())

    def _asyncgen_firstiter_hook(self, agen):
        if self._asyncgens_shutdown_called:
            warnings.warn(
                f"asynchronous generator {agen!r} was scheduled after "
                f"loop.shutdown_asyncgens() call",
                ResourceWarning,
                source=self,
            )

        self._asyncgens.add(agen)

    async def shutdown_asyncgens(self):
        return
        """Shutdown all active asynchronous generators."""
        self._asyncgens_shutdown_called = True

        if not len(self._asyncgens):
            # If Python version is <3.6 or we don't have any asynchronous
            # generators alive.
            return

        closing_agens = list(self._asyncgens)
        self._asyncgens.clear()

        results = await tasks.gather(
            *[ag.aclose() for ag in closing_agens], return_exceptions=True
        )

        for result, agen in zip(results, closing_agens):
            if isinstance(result, Exception):
                self.call_exception_handler(
                    {
                        "message": f"an error occurred during closing of "
                        f"asynchronous generator {agen!r}",
                        "exception": result,
                        "asyncgen": agen,
                    }
                )


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
