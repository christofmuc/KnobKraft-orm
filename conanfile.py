from conans import ConanFile
from os import path


class KnobKraftOrm(ConanFile):
    settings = ["os", "compiler", "build_type", "arch"]
    requires = ["sentry-native/0.4.12", "winsparkle/0.6.0"]
    generators = "cmake", "cmake_find_package", "json", "virtualenv"

    def configure(self):
        if self.settings.os == "Windows":
            self.requires.add("pdcurses/3.9")
        if self.settings.os == "Macos" or self.settings.os == "Windows":
            self.requires.add("sentry-crashpad/0.4.12")

    def imports(self):
      # This will copy the DLLs from conan packages
      self.copy("*.dll", dst=".", src="bin") 
      # For Linux, copy the .so files
      self.copy("*.so.*", dst=".", src="lib")
      # Make crashpad_handler to be found
      if self.settings.os == "Macos":
        self.copy("crashpad_handler", src="bin", dst=".")
      if self.settings.os == "Windows":
        # This is more tricky, as Windows does only put the crashpad_handler.exe on the path, but does not install it
        # in the package. We will just loop the defined paths (glanced from by debugging conan) and copy it
        # once we find it!
        for p in self.deps_cpp_info.bin_paths:
            if path.isfile(path.join(p, "crashpad_handler.exe")):
                self.copy("crashpad_handler.exe", src=p, dst=".")
