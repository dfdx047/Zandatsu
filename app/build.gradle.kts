import java.util.Properties

plugins {
    id("com.android.application")
}

// CMake: por omissao usa o CMake do sistema (o mesmo que compilou o SDK no PC),
// indicado em local.properties com "cmake.dir=/usr". A versao e lida do proprio
// binario, porque o Gradle exige a versao exata.
val localProperties = Properties().apply {
    val file = rootProject.file("local.properties")
    if (file.exists()) file.inputStream().use { load(it) }
}
val cmakeDir: String? = localProperties.getProperty("cmake.dir")
val cmakeVersionFromDir: String? = cmakeDir?.let { dir ->
    val output = providers.exec {
        commandLine("$dir/bin/cmake", "--version")
    }.standardOutput.asText.get()
    Regex("""cmake version (\d+\.\d+\.\d+)""").find(output)?.groupValues?.get(1)
}

// "./gradlew assembleRelease -Psm.uiOnly=true" compila so o launcher (Java), sem o
// jogo nativo: util para mexer na interface sem esperar pelo NDK.
val uiOnly = (project.findProperty("sm.uiOnly") as String?) == "true"
// "-Psm.d3dTrace=true": compila os hooks de rastreio das funcoes D3D
// (app/src/main/cpp/d3d_trace_hooks.cpp). So para mapear a biblioteca D3D.
val d3dTrace = (project.findProperty("sm.d3dTrace") as String?) == "true"

android {
    namespace = "com.dfdx047.swordman"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.dfdx047.swordman"
        // API 30 (Android 11): memfd, posix_spawn, strtod_l e ASharedMemory no bionic.
        minSdk = 30
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        if (!uiOnly) {
            externalNativeBuild {
                cmake {
                    arguments += listOf(
                        "-DANDROID_STL=c++_shared",
                        // O runtime e sempre compilado em Release: em Debug o jogo
                        // recompilado fica lento demais para testar.
                        "-DCMAKE_BUILD_TYPE=Release",
                        "-DREXGLUE_ENABLE_TRACY=OFF",
                        "-DREXGLUE_BUILD_TESTS=OFF",
                        "-DSM_D3D_TRACE=" + (if (d3dTrace) "ON" else "OFF"),
                    )
                    targets += listOf(
                        "main", "rexruntime", "rexgpu-xenos", "SDL3-shared",
                        // Hooks do libadrenotools (driver Vulkan alternativo, ex. Turnip).
                        "main_hook", "file_redirect_hook", "gsl_alloc_hook", "hook_impl",
                    )
                }
            }
        }
    }

    if (!uiOnly) {
        externalNativeBuild {
            cmake {
                path = file("src/main/cpp/CMakeLists.txt")
                version = cmakeVersionFromDir ?: "3.31.6"
            }
        }
    }

    sourceSets {
        getByName("main") {
            // Classes Java do SDL (SDLActivity e companhia), direto do submodulo do
            // SDK, para ficarem sempre na mesma versao que o libSDL3.so.
            java.srcDir("../external/rexglue-sdk-0.10/thirdparty/sdl3/android-project/app/src/main/java")
        }
    }

    packaging {
        jniLibs {
            // As .so ficam extraidas em nativeLibraryDir: o SDL abre libmain.so por
            // caminho e o runtime carrega o plugin da GPU de la.
            useLegacyPackaging = true
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            // Assinado com a chave de debug por enquanto, para instalar direto.
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    buildFeatures {
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
