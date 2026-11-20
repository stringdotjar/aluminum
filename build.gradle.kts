// Shared setup for every Java module. The C runtime in aluminum-runtime is built with CMake, not Gradle.
subprojects {
  apply(plugin = "java")

  extensions.configure<JavaPluginExtension> {
    toolchain {
      languageVersion = JavaLanguageVersion.of(25)
    }
  }

  tasks.withType<JavaCompile>().configureEach {
    options.encoding = "UTF-8"
    options.compilerArgs.addAll(listOf("-Xlint:all", "-Werror"))
  }

  tasks.withType<Test>().configureEach {
    useJUnitPlatform()
  }
}
