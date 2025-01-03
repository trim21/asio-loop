from __future__ import annotations

import asyncio
import asyncio.log
import ssl
from typing import Callable, TypeAlias

ProtocolFactory: TypeAlias = Callable[[], asyncio.BaseProtocol]
ServerSSLContext: TypeAlias = None | ssl.SSLContext
