/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <string>

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/pybind11.h>
#ifdef _MSC_VER
#pragma warning(pop)
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
	pybind11::object _stdout;
	pybind11::object _stderr;
	pybind11::object _stdout_buffer;
	pybind11::object _stderr_buffer;
};
