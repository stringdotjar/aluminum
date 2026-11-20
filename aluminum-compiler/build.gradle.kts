plugins {
  application
}

dependencies {
  implementation(project(":aluminum-annotations"))

  testImplementation(platform(libs.junit.bom))
  testImplementation(libs.junit.jupiter)
  testRuntimeOnly(libs.junit.platform.launcher)
}

application {
  mainClass = "io.github.stringdotjar.aluminum.compiler.Aluminum"
}
