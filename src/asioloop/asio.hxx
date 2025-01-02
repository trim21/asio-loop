#pragma once

#include "common.hxx"

#if WIN32
#include <SDKDDKVer.h>
#endif

#include <boost/asio.hpp>

#include "boost/asio/awaitable.hpp"
#include "boost/asio/ip/basic_resolver.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/use_awaitable.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#if WIN32
#include <stringapiset.h>
#include <windows.h>
#endif
