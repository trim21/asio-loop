class EventLoop:
    name: str
    def __init__(self, name: str) -> None: ...
    def call_soon(self, callback, *args, context=None) -> None: ...
    def run_forever(self) -> None: ...
