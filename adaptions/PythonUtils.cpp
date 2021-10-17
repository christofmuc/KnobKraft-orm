/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PythonUtils.h"

#include "Logger.h"

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace py = pybind11;

PyStdErrOutStreamRedirect::PyStdErrOutStreamRedirect()
{
	clear();
}

PyStdErrOutStreamRedirect::~PyStdErrOutStreamRedirect()
{
	py::gil_scoped_acquire acquire;
	auto sysm = py::module::import("sys");
	sysm.attr("stdout") = _stdout;
	sysm.attr("stderr") = _stderr;
}

std::string PyStdErrOutStreamRedirect::stdoutString()
{
	py::gil_scoped_acquire acquire;
	_stdout_buffer.attr("seek")(0);
	return py::str(_stdout_buffer.attr("read")());
}

std::string PyStdErrOutStreamRedirect::stderrString()
{
	py::gil_scoped_acquire acquire;
	_stderr_buffer.attr("seek")(0);
	return py::str(_stderr_buffer.attr("read")());
}

void PyStdErrOutStreamRedirect::clear()
{	
	py::gil_scoped_acquire acquire;
	auto sysm = py::module::import("sys");
	_stdout = sysm.attr("stdout");
	_stderr = sysm.attr("stderr");
	auto stringio = py::module::import("io").attr("StringIO");
	_stdout_buffer = stringio();  // Other file like object can be used here as well, such as objects created by pybind11
	_stderr_buffer = stringio();
	sysm.attr("stdout") = _stdout_buffer;
	sysm.attr("stderr") = _stderr_buffer;
}

void PyStdErrOutStreamRedirect::flushToLogger(std::string const &logDomain)
{
	auto error = stderrString();
	if (!error.empty()) {
		SimpleLogger::instance()->postMessage(logDomain + " ERROR: " + error);
	}
	auto output = stdoutString();
	if (!output.empty()) {
		boost::trim_right(output);
		SimpleLogger::instance()->postMessage((boost::format("%s: %s") % logDomain % output).str());
	}
	clear();
}

