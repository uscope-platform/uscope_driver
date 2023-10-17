from conan import ConanFile
from conan.errors import ConanInvalidConfiguration


class BasicConanfile(ConanFile):
    name = "uscope_driver"
    version = "1.0"
    description = "uscope_driver recipe"
    license = "Apache-2.0"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("cli11/2.3.2")
        self.requires("nlohmann_json/3.11.2")
        self.requires("json-schema-validator/2.2.0")
        if self.settings.arch == "x86_64":
            self.requires("libfuse/3.10.5")


    def build_requirements(self):
        pass

    def generate(self):
        pass

    def build(self):
        pass

    def package(self):
        pass
