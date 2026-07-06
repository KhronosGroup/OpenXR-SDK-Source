// Copyright 2025-2026 The Khronos Group Inc.
// Copyright 2021-2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
plugins {
    id("maven-publish")
    signing
    id("com.vanniktech.maven.publish.base") version "0.34.0"
}

// These next few lines are just to make the version match the OpenXR release.
val root by extra(file("../.."))
project.ext["repoRoot"] = root
apply(file(File(root, "src/version.gradle")))

version = project.ext["versionOpenXR"].toString() + project.ext["versionQualifier"]

val siteUrl = "https://github.com/KhronosGroup/OpenXR-SDK-Source"
val gitUrl = "scm:git:git@github.com:KhronosGroup/OpenXR-SDK-Source.git"

signing {
    val signingKeyId: String? by project
    val signingKey: String? by project
    val signingPassword: String? by project
    if (signingKeyId != null) {
        useInMemoryPgpKeys(signingKeyId, signingKey, signingPassword)
        sign(publishing.publications)
    }
}

val loaderAar = file("$root/openxr_loader_for_android-${version}.aar")
val loaderSourcesJar = file("$root/openxr_loader_for_android-${version}-sources.jar")
val apiDumpLayerAar = file("$root/apilayer_api_dump-${version}.aar")
val coreValidationLayerAar = file("$root/apilayer_core_validation-${version}.aar")
val bestPracticesValidationLayerAar = file("$root/apilayer_best_practices_validation-${version}.aar")

publishing {
    publications {
        create<MavenPublication>("maven") {

            artifactId = "openxr_loader_for_android"

            artifacts {
                artifact(loaderAar) {
                    extension = "aar"
                }

                artifact(loaderSourcesJar) {
                    extension = "jar"
                    classifier = "sources"
                }
            }

            pom {
                name.set("OpenXR Loader for Android")
                description.set(
                    """
                    |The AAR for the OpenXR Loader as used on Android, along with required global manifest entries.
                    |You still need to apply changes to the application tag and the activity tag corresponding to your OpenXR experience.""".trimMargin()
                )
                url.set(siteUrl)
                licenses {
                    license {
                        name.set("Apache-2.0")
                        url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                    }
                    // or MIT, but "OR" is not easy to express clearly in POM.
                    // license {
                    //     name.set("MIT")
                    //     url.set("https://spdx.org/licenses/MIT.txt")
                    // }
                }
                developers {
                    developer {
                        name.set("The Khronos Group, Inc. OpenXR Working Group")
                        email.set("openxr-speceditor AT khronos DOT org")
                        url.set("https://khronos.org/openxr")
                    }
                }
                scm {
                    connection.set("scm:git:${siteUrl}.git")
                    developerConnection.set(gitUrl)
                    url.set(siteUrl)
                }
                issueManagement {
                    system.set("GitHub Issues")
                    url.set("${siteUrl}/issues")
                }
            }
        }

        create<MavenPublication>("apiDumpLayer") {

            artifactId = "apilayer_api_dump"

            artifacts {
                artifact(apiDumpLayerAar) {
                    extension = "aar"
                }
            }

            pom {
                name.set("OpenXR api_dump Layer")
                description.set("An API layer to dump out OpenXR API calls for debugging during the development process.")
                url.set(siteUrl)
                licenses {
                    license {
                        name.set("Apache-2.0")
                        url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                    }
                }
                developers {
                    developer {
                        name.set("The Khronos Group, Inc. OpenXR Working Group")
                        email.set("openxr-speceditor AT khronos DOT org")
                        url.set("https://khronos.org/openxr")
                    }
                }
                scm {
                    connection.set("scm:git:${siteUrl}.git")
                    developerConnection.set(gitUrl)
                    url.set(siteUrl)
                }
                issueManagement {
                    system.set("GitHub Issues")
                    url.set("${siteUrl}/issues")
                }
            }
        }
        create<MavenPublication>("coreValidationLayer") {

            artifactId = "apilayer_core_validation"

            artifacts {
                artifact(coreValidationLayerAar) {
                    extension = "aar"
                }
            }

            pom {
                name.set("OpenXR core_validation Layer")
                description.set("An API layer to verify valid usage during the development process.")
                url.set(siteUrl)
                licenses {
                    license {
                        name.set("Apache-2.0")
                        url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                    }
                }
                developers {
                    developer {
                        name.set("The Khronos Group, Inc. OpenXR Working Group")
                        email.set("openxr-speceditor AT khronos DOT org")
                        url.set("https://khronos.org/openxr")
                    }
                }
                scm {
                    connection.set("scm:git:${siteUrl}.git")
                    developerConnection.set(gitUrl)
                    url.set(siteUrl)
                }
                issueManagement {
                    system.set("GitHub Issues")
                    url.set("${siteUrl}/issues")
                }
            }
        }
        create<MavenPublication>("bestPracticesValidationLayer") {

            artifactId = "apilayer_best_practices_validation"

            artifacts {
                artifact(bestPracticesValidationLayerAar) {
                    extension = "aar"
                }
            }

            pom {
                name.set("OpenXR best_practices_validation Layer")
                description.set("An API layer to check for best practices usage during the development process.")
                url.set(siteUrl)
                licenses {
                    license {
                        name.set("Apache-2.0")
                        url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                    }
                }
                developers {
                    developer {
                        name.set("The Khronos Group, Inc. OpenXR Working Group")
                        email.set("openxr-speceditor AT khronos DOT org")
                        url.set("https://khronos.org/openxr")
                    }
                }
                scm {
                    connection.set("scm:git:${siteUrl}.git")
                    developerConnection.set(gitUrl)
                    url.set(siteUrl)
                }
                issueManagement {
                    system.set("GitHub Issues")
                    url.set("${siteUrl}/issues")
                }
            }
        }
        repositories {
            maven {
                name = "BuildDir"
                url = uri(layout.buildDirectory.dir("repo"))
            }

            if (version.toString().endsWith("SNAPSHOT")) {
                maven {
                    name = "Snapshots"
                    url = uri("https://central.sonatype.com/repository/maven-snapshots")
                    credentials {

                        val mavenCentralUsername: String? by project
                        val mavenCentralPassword: String? by project
                        username = mavenCentralUsername
                        password = mavenCentralPassword
                    }
                }
            }

        }
    }
}
mavenPublishing {
    publishToMavenCentral()
}
