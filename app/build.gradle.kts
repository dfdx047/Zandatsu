import java.util.Properties
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// CMake: o Gradle exige a versao exata. Ordem: "cmake.dir" do local.properties;
// senao o cmake do PATH (o do sistema, ex. /usr/bin/cmake), cuja versao e lida do
// proprio binario; senao o 3.31.6 do SDK Android (SDK Manager > CMake).
val localProperties = Properties().apply {
    val file = rootProject.file("local.properties")
    if (file.exists()) file.inputStream().use { load(it) }
}
val cmakeDir: String? = localProperties.getProperty("cmake.dir")
fun cmakeVersionOf(binary: String): String? = try {
    val output = providers.exec {
        commandLine(binary, "--version")
        isIgnoreExitValue = true
    }.standardOutput.asText.get()
    Regex("""cmake version (\d+\.\d+\.\d+)""").find(output)?.groupValues?.get(1)
} catch (e: Exception) {
    null
}
val cmakeVersionFromDir: String? =
    if (cmakeDir != null) cmakeVersionOf("$cmakeDir/bin/cmake") else cmakeVersionOf("cmake")

// "./gradlew assembleRelease -Pdf.uiOnly=true" compila so o launcher (Kotlin),
// sem o jogo nativo: util para mexer na interface sem esperar pelo NDK.
// O APK resultante abre o launcher mas nao consegue arrancar o jogo.
val uiOnly = (project.findProperty("df.uiOnly") as String?) == "true"
// "-Pdf.d3dTrace=true": compila os hooks de rastreio das funcoes D3D
// (app/src/main/cpp/d3d_trace_hooks.cpp, gerado por tools/d3d_trace_gen.py).
// So para mapear a biblioteca D3D; nao usar em testes de desempenho.
val d3dTrace = (project.findProperty("df.d3dTrace") as String?) == "true"
// "-Pdf.thinlto=true": ThinLTO no codigo gerado e no libmain.so (SM_THINLTO; build a parte, mais lento).
val thinLto = (project.findProperty("df.thinlto") as String?) == "true"

android {
    namespace = "com.dfdx047.swordman"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.dfdx047.swordman"
        // API 30 (Android 11): memfd, posix_spawn, strtod_l e ASharedMemory
        // estao todos disponiveis no bionic.
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
                        // CMake 4 recusa cmake_minimum_required < 3.5 (submodulos antigos).
                        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
                        "-DREXGLUE_ENABLE_TRACY=OFF",
                        "-DREXGLUE_BUILD_TESTS=OFF",
                        "-DDB_D3D_TRACE=" + (if (d3dTrace) "ON" else "OFF"),
                        "-DSM_THINLTO=" + (if (thinLto) "ON" else "OFF"),
                    )
                    // Frame pointers em todo o codigo nativo: o vigia (watchdog.cpp) e o
                    // simpleperf seguem a cadeia de frames para mostrar as pilhas.
                    cFlags += "-fno-omit-frame-pointer"
                    cppFlags += "-fno-omit-frame-pointer"
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
            // Classes Java do SDL (SDLActivity e companhia), direto do submodulo
            // do SDK, para ficarem sempre na mesma versao que o libSDL3.so.
            java.srcDir("../external/rexglue-sdk/thirdparty/sdl3/android-project/app/src/main/java")
        }
    }

    packaging {
        jniLibs {
            // As .so ficam extraidas em nativeLibraryDir: o SDL abre libmain.so
            // por caminho e o runtime carrega o plugin da GPU de la.
            useLegacyPackaging = true
        }
    }

    // Chave de release: keystore.properties na raiz do projeto (fora do git) com
    // storeFile, storePassword, keyAlias, keyPassword. Sem esse arquivo, usa a chave de debug.
    val releaseKeyFile = rootProject.file("keystore.properties")
    if (releaseKeyFile.exists()) {
        val keyProps = Properties().apply { releaseKeyFile.inputStream().use { load(it) } }
        signingConfigs.create("release") {
            storeFile = rootProject.file(keyProps.getProperty("storeFile"))
            storePassword = keyProps.getProperty("storePassword")
            keyAlias = keyProps.getProperty("keyAlias")
            keyPassword = keyProps.getProperty("keyPassword")
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            signingConfig = if (releaseKeyFile.exists()) signingConfigs.getByName("release")
                else signingConfigs.getByName("debug")
        }
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2025.09.00")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended:1.7.8")
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("androidx.core:core-ktx:1.16.0")
    implementation("androidx.documentfile:documentfile:1.1.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.2")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.9.2")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    debugImplementation("androidx.compose.ui:ui-tooling")
}

// O codigo nativo vem de fora do modulo (external/rexglue-sdk com os patches, generated/default
// do codegen): o Gradle nao ve essas mudancas e marcava a compilacao nativa como "up-to-date",
// gerando um APK com as .so antigas. Assim o ninja corre sempre (ele proprio e incremental:
// sem mudancas leva poucos segundos).
tasks.configureEach {
    if (name.startsWith("buildCMake") || name.startsWith("externalNativeBuild") ||
        name.startsWith("configureCMake")
    ) {
        outputs.upToDateWhen { false }
    }
}
