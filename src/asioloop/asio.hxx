#pragma once

#include "common.hxx"

#if defined(_MSC_VER)
#define OS_WIN32 1
#include <SDKDDKVer.h>
#endif

#include <boost/asio.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>

#if OS_WIN32
#include <stringapiset.h>
#include <windows.h>
#endif
