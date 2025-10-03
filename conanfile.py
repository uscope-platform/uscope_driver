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
        self.requires("cli11/2.5.0")
        self.requires("nlohmann_json/3.12.0")
        self.requires("valijson/1.0.3")
        self.requires("gtest/1.17.0")
        self.requires("spdlog/1.15.3")
        self.requires("antlr4-cppruntime/4.13.2")
        self.requires("asio/1.36.0")
        self.requires("cppcodec/0.2")

    def build_requirements(self):
        pass

    def generate(self):
        pass

    def build(self):
        pass

    def package(self):
        pass
