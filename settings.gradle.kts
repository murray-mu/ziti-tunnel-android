/*
 * Copyright (c) 2020 NetFoundry. All rights reserved.
 */

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

include(":tunnel")
include(":app")
project(":app").name = "ziti-mobile-edge"

