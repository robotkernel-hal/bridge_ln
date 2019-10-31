from conans import ConanFile, AutoToolsBuildEnvironment, tools
import re

class MainProject(ConanFile):
    name = "bridge_ln"
    license = "GPLv3"
    url = f"https://rmc-github.robotic.dlr.de/robotkernel/{name}"
    description = "robotkernel-5 is a modular, easy configurable hardware abstraction framework"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = ["*", "!.gitignore"] + ["!%s" % x for x in tools.Git().excluded_files()]

    generators = "pkg_config"
    requires = (
            "robotkernel/[~=5.0]@robotkernel/stable",
            "liblinks_and_nodes/master@common/snapshot" )

    def source(self):
        filedata = None
        filename = "project.properties"
        with open(filename, 'r') as f:
            filedata = f.read()
        with open(filename, 'w') as f:
            f.write(re.sub("VERSION *=.*[^\n]", f"VERSION = {self.version}", filedata))

    def build(self):
        self.run("autoreconf -if")
        autotools = AutoToolsBuildEnvironment(self)
        autotools.libs=[]
        autotools.include_paths=[]
        autotools.library_paths=[]
        if self.settings.build_type == "Debug":
            autotools.flags = ["-O0", "-g"]
        else:
            autotools.flags = ["-O3"]
        autotools.configure(configure_dir=".")
        autotools.make()

    def package(self):
        autotools = AutoToolsBuildEnvironment(self)
        autotools.install()

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        self.cpp_info.bindirs = ['bin']
        self.cpp_info.resdirs = ['share']
