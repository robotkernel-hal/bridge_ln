from conan import ConanFile


class MainProject(ConanFile):
    python_requires = "conan_template/[~5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "bridge_ln"
    description = "The ln bridge exports robotkernel services via links-and-nodes."
    exports_sources = ["*", "!.gitignore"]

    tool_requires = "links_and_nodes_base_python/[>=1.2.3 <3]@common/stable"

    def requirements(self):
        self.requires("robotkernel/6.0.0-vec-rework@robotkernel/snapshot")
        self.requires("libstring_util/[~1]@common/stable")
        self.requires("liblinks_and_nodes/[>=1.2.3 <3]@common/stable")
        self.requires("ln_helper/[~0]@robotkernel/stable")
    
    def source(self):
        self.run(f"sed 's/AC_INIT(.*/AC_INIT([bridge_ln], [{self.version}], [{self.author}])/' configure.ac.in > configure.ac")

