from conans import tools, python_requires

base = python_requires("conan_template/[~=5]@robotkernel/stable")

class MainProject(base.RobotkernelConanFile):
    name = "bridge_ln"
    description = "The ln bridge exports robotkernel services via links-and-nodes."
    exports_sources = ["*", "!.gitignore"] + ["!%s" % x for x in tools.Git().excluded_files()]

    def requirements(self):
        self.requires("liblinks_and_nodes/[~=1]@common/stable")
    
    def build_requirements(self):
        self.build_requires("robotkernel/[~=5]@robotkernel/stable")
        self.build_requires("liblinks_and_nodes/[~=1]@common/stable")

