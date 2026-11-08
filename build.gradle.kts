import org.gradle.api.DefaultTask
import org.gradle.api.file.RegularFileProperty
import org.gradle.api.provider.Property
import org.gradle.api.tasks.CacheableTask
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.InputFile
import org.gradle.api.tasks.OutputFile
import org.gradle.api.tasks.PathSensitive
import org.gradle.api.tasks.PathSensitivity
import org.gradle.api.tasks.TaskAction
import org.gradle.process.ExecOperations
import java.io.File
import javax.inject.Inject

@CacheableTask
abstract class CompileCmakeTask : DefaultTask() {
    @get:Inject abstract val execOps: ExecOperations
    @get:InputFile @get:PathSensitive(PathSensitivity.NONE) abstract val cmakeFile: RegularFileProperty
    @get:InputFile @get:PathSensitive(PathSensitivity.NONE) abstract val sourceFile: RegularFileProperty
    @get:InputFile @get:PathSensitive(PathSensitivity.NONE) abstract val headerFile: RegularFileProperty
    @get:Input abstract val ndkDir: Property<String>
    @get:OutputFile abstract val outputFile: RegularFileProperty

    @TaskAction
    fun build() {
        val ndk = File(ndkDir.get())
        val os = System.getProperty("os.name").lowercase()
        val tag = if ("win" in os) "windows-x86_64" else if ("mac" in os) "darwin-x86_64" else "linux-x86_64"
        val prebuilt = ndk.resolve("toolchains/llvm/prebuilt/$tag")
        val sysroot = prebuilt.resolve("sysroot")
        val rev = ndk.resolve("source.properties").takeIf { it.isFile }?.readLines()
            ?.firstOrNull { it.startsWith("Pkg.Revision") }?.substringAfter('=')?.trim() ?: ndk.name
        val ndkMajor = rev.substringBefore('.').toIntOrNull() ?: 0
        val api = if (ndkMajor >= 30) 37 else 35
        val out = outputFile.get().asFile
        val cmakeDir = out.parentFile.resolve("cmake-build").apply { mkdirs() }
        val clang23 = System.getenv("PATH")?.split(File.pathSeparator)
            ?.map { File(it, "clang-23") }?.firstOrNull { it.canExecute() }
        val compiler = clang23?.absolutePath
            ?: prebuilt.resolve("bin/aarch64-linux-android$api-clang${if ("win" in os) ".cmd" else ""}").absolutePath
        val rtDir = prebuilt.resolve("lib/clang").listFiles()?.firstOrNull { it.isDirectory }?.resolve("lib/linux")
        val targetFlags = "--target=aarch64-linux-android$api --sysroot=${sysroot.absolutePath}"
        val linkFlags = "$targetFlags -nodefaultlibs" +
            (rtDir?.let { " -L${it.absolutePath} -l:libclang_rt.builtins-aarch64-android.a" } ?: "") +
            " -lc"

        execOps.exec {
            commandLine(
                "cmake",
                "-S", cmakeFile.get().asFile.parentFile.absolutePath,
                "-B", cmakeDir.absolutePath,
                "-DCMAKE_SYSTEM_NAME=Linux",
                "-DCMAKE_SYSTEM_PROCESSOR=aarch64",
                "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
                "-DCMAKE_C_COMPILER=$compiler",
                "-DCMAKE_C_FLAGS=$targetFlags",
                "-DCMAKE_SHARED_LINKER_FLAGS=$linkFlags",
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${out.parentFile.absolutePath}"
            )
        }
        execOps.exec { commandLine("cmake", "--build", cmakeDir.absolutePath, "--parallel") }
    }
}

val outSo = layout.buildDirectory.file("native/libhostsredirect.so")
val ndk = providers.environmentVariable("ANDROID_NDK_HOME")
    .orElse(providers.environmentVariable("ANDROID_NDK_ROOT"))
    .orElse(
        providers.environmentVariable("ANDROID_HOME")
            .orElse(providers.environmentVariable("ANDROID_SDK_ROOT"))
            .map { sdk ->
                File(sdk, "ndk").listFiles()?.filter { it.isDirectory }
                    ?.maxByOrNull { it.name.substringBefore('.').toIntOrNull() ?: 0 }?.absolutePath
                    ?: error("No NDK found under $sdk/ndk")
            }
    )

val buildNative = tasks.register<CompileCmakeTask>("buildNative") {
    cmakeFile.set(layout.projectDirectory.file("CMakeLists.txt"))
    sourceFile.set(layout.projectDirectory.file("src/hook.c"))
    headerFile.set(layout.projectDirectory.file("src/zygisk_next_api.h"))
    ndkDir.set(ndk)
    outputFile.set(outSo)
}

tasks.register<Zip>("zipRelease") {
    dependsOn(buildNative)
    archiveFileName.set("ZN-hostsredirect-v4.1-release.zip")
    destinationDirectory.set(layout.projectDirectory.dir("release"))
    from(layout.projectDirectory.dir("template"))
    from(outSo) { into("lib") }
}
