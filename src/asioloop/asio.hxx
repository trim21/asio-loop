#pragma once

#include "common.hxx"

#if OS_WIN32
#include <SDKDDKVer.h>
#endif

#include <boost/asio.hpp>

#if OS_WIN32
#include <stringapiset.h>
#include <windows.h>
#endif
