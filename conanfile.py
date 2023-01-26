from conans import ConanFile, tools

class MainProject(ConanFile):
    python_requires = "conan_template/[~=5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "bridge_ln"
    description = "The ln bridge exports robotkernel services via links-and-nodes."
    exports_sources = ["*", "!.gitignore"] + ["!%s" % x for x in tools.Git().excluded_files()]

    def requirements(self):
        self.requires("libstring_util/[~=1]@common/stable")
        self.requires("liblinks_and_nodes/[>=1.2.3 <3]@common/stable") 
    
    def build_requirements(self):
        self.build_requires("ln_helper/master@robotkernel/snapshot")
        self.build_requires("robotkernel/[~=5]@robotkernel/stable")

