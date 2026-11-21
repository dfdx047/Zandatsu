import java.util.Properties
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// Zandatsu PC: app so do port de PC (pc/), sem o 360 (sem CMake, sem libgame). Empacota o que o build do PC ja gerou
// fora do git:
//   <pcBuild>/assets/   libmgrpc.zdat + libmgrpc.json (codigo do jogo encriptado, pc/tools/pack-libmgr.py)
//   <pcBuild>/jnilibs/  libmgrlab.so (carregador) e libdxvk_android.so
//   <hooks>/            hooks do libadrenotools (libmain_hook, libfile_redirect_hook, libgsl_alloc_hook,
//                       libhook_impl), compilados pelo CMake da app do 360 (driver Vulkan alternativo).
// Por omissao: <raiz>/build/pc e <raiz>/app/build/intermediates/cmake/release/obj/arm64-v8a. Num worktree sem build/:
//   ./gradlew :pcapp:assembleRelease -Pdf.pcBuild=/caminho/ProjectSwordMan/build/pc -Pdf.pcHooks=/caminho/.../arm64-v8a
// (ou pc.build / pc.hooks no local.properties).
val localProperties = Properties().apply {
    val file = rootProject.file("local.properties")
    if (file.exists()) file.inputStream().use { load(it) }
}
fun setting(gradleName: String, localName: String): String? =
    (project.findProperty(gradleName) as String?) ?: localProperties.getProperty(localName)
val pcBuild: File = setting("df.pcBuild", "pc.build")?.let { File(it) } ?: rootProject.file("build/pc")
val pcHooks: File = setting("df.pcHooks", "pc.hooks")?.let { File(it) }
    ?: rootProject.file("app/build/intermediates/cmake/release/obj/arm64-v8a")
// libhook_impl.so precisa do libc++_shared.so (STL do CMake da app do 360), copiado da mesma pasta.
val hookNames = listOf(
    "libmain_hook.so", "libfile_redirect_hook.so", "libgsl_alloc_hook.so", "libhook_impl.so", "libc++_shared.so",
)
val hooksOut = layout.buildDirectory.dir("pc-hooks")

android {
    namespace = "com.dfdx047.zandatsu.pc"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.dfdx047.zandatsu.pc"
        // API 30: memfd_create (Os.memfd_create) e android_dlopen_ext por fd.
        minSdk = 30
        targetSdk = 35
        versionCode = 1
        versionName = "0.1"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDir(File(pcBuild, "assets"))
            jniLibs.srcDir(File(pcBuild, "jnilibs"))
            jniLibs.srcDir(hooksOut)
        }
    }

    packaging {
        jniLibs {
            // As .so ficam extraidas em nativeLibraryDir: o carregador abre os hooks do adrenotools por caminho
            // (MGR_HOOK_DIR) e o NativeActivity carrega libmgrlab.so de la.
            useLegacyPackaging = true
            // O codigo do jogo so vai encriptado (assets/libmgrpc.zdat), nunca como .so.
            excludes += listOf("**/libgame*.so", "**/libmgrpc*.so")
        }
    }

    // Mesma chave de release da app do 360 (keystore.properties na raiz, fora do git); sem ela, a de debug.
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
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
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
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended:1.7.8")
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("androidx.core:core-ktx:1.16.0")
    implementation("androidx.documentfile:documentfile:1.1.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.2")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    // libmgrpc.zdat ZDT3: blocos xz.
    implementation("org.tukaani:xz:1.10")
}

// Hooks do adrenotools: copiados do build da app do 360 para build/pc-hooks/arm64-v8a (jniLibs). Sem eles o jogo so
// consegue usar o driver Vulkan do sistema; o build avisa mas continua.
val copyPcHooks by tasks.registering(Copy::class) {
    from(pcHooks) { include(hookNames) }
    into(hooksOut.map { it.dir("arm64-v8a") })
    doFirst {
        val missing = hookNames.filterNot { File(pcHooks, it).isFile }
        if (missing.isNotEmpty()) logger.warn("pcapp: faltam hooks do adrenotools em $pcHooks: $missing (so o driver do sistema)")
    }
}

tasks.configureEach {
    if (Regex("merge\\w*JniLibFolders").matches(name)) dependsOn(copyPcHooks)
    if (Regex("merge\\w*Assets").matches(name)) doFirst {
        if (!File(pcBuild, "assets/libmgrpc.zdat").isFile) logger.warn("pcapp: falta $pcBuild/assets/libmgrpc.zdat (a app nao arranca o jogo)")
    }
}
