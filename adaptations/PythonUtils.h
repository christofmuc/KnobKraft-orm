/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <string>
#include <pybind11/pybind11.h>

#ifndef DEFAULT_VISIBILITY
#ifdef __GNUC__
#define DEFAULT_VISIBILITY __attribute__((visibility("default")))
#else
#define DEFAULT_VISIBILITY 
#endif
#endif

class PyStdErrOutStreamRedirect {
public:
	PyStdErrOutStreamRedirect();
	~PyStdErrOutStreamRedirect();

	std::string stdoutString();
	std::string stderrString();

	void clear();
	void flushToLogger(std::string const &logDomain);

private:
	pybind11::object _stdout DEFAULT_VISIBILITY;
	pybind11::object _stderr DEFAULT_VISIBILITY;
	pybind11::object _stdout_buffer DEFAULT_VISIBILITY;
	pybind11::object _stderr_buffer DEFAULT_VISIBILITY;
};
