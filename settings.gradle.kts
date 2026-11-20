plugins {
  id("org.gradle.toolchains.foojay-resolver-convention") version "1.0.0"
}

dependencyResolutionManagement {
  repositories {
    mavenCentral()
  }
}

rootProject.name = "aluminum"

include("aluminum-annotations", "aluminum-compiler")
