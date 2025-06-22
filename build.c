//due to cmake not actually building the project, and just generating vsproj files on windows, windows is borked, linux works though
#include <stdio.h>
#define BUILD_IMPLEMENTATION
#include "build.h"


#ifdef _MSC_VER
  #define LIB ".lib"
#else
  #define LIB ".a"
#endif

#ifdef _MSC_VER
    #define PLATFORM_LIBS FlagArray( \
        FLAG_RAW("opengl32.lib"), \
        FLAG_RAW("user32.lib"), \
        FLAG_RAW("gdi32.lib"), \
        FLAG_RAW("kernel32.lib"), \
        FLAG_RAW("shell32.lib"), \
        FLAG_RAW("advapi32.lib"), \
        FLAG_RAW("ws2_32.lib"), \
        FLAG_RAW("ole32.lib"), \
        FLAG_RAW("uuid.lib") \
    ), 9
#else
    #define PLATFORM_LIBS FlagArray( \
        FLAG_RAW("-lwayland-client"), \
        FLAG_RAW("-lwayland-cursor"), \
        FLAG_RAW("-lwayland-egl"), \
        FLAG_RAW("-lEGL"), \
        FLAG_RAW("-lGL"), \
        FLAG_RAW("-lm"), \
        FLAG_RAW("-lpthread"), \
        FLAG_RAW("-ldl") \
    ), 8
#endif



void compile_cmake(string name, string winsln) {
    char build_dir[BufferSize] = {'\0'};
    sprintf(build_dir, "./deps/%s/build/", name);
    if(!Build.fs.exists(build_dir)) {
        Build.fs.mkdir(build_dir);
        char cmd[BufferSize] ={'\0'};
        sprintf(cmd, "cmake -S ./deps/%s -B ./deps/%s/build/ -DCGLM_SHARED=OFF -DCGLM_STATIC=ON", name, name);
        system(cmd);
        cmd[0] = '\0';
        sprintf(cmd, "cmake --build ./deps/%s/build/", name);
        system(cmd);
#ifdef _WIN32
        cmd[0] = '\0';
        sprintf(cmd, "msbuild /deps/%s/build/%s.sln p:Configuration=Release", name, winsln);
        system(cmd);
#endif
    } else {
        printf("not rebuilding %s\n", name);
    }
}

void compile_asset(string in, string out) {
    char cmd [BufferSize] = {'\0'};
    if(__Build_needs_rebuild__(out, StringArray(in), 1)) {
        sprintf(cmd, "xxd -i %s > %s", in, out);
        printf("running %s\n", cmd);
        system(cmd);
        printf("done\n");
    } else {
        printf("not rebuilding %s\n", out);
    }
}

void make_assets() {
    if(!Build.fs.exists("./target/assets")) {
        Build.fs.mkdir("./target/assets");
    }
    if(!Build.fs.exists("./target/assets/shaders")) {
        Build.fs.mkdir("./target/assets/shaders");
    }
    compile_asset("./assets/shaders/frag.glsl", "./target/assets/shaders/frag.h");
    compile_asset("./assets/shaders/vert.glsl", "./target/assets/shaders/vert.h");
    compile_asset("./assets/shaders/geo.glsl", "./target/assets/shaders/geo.h");
    if(!Build.fs.exists("./target/assets/textures")) {
        Build.fs.mkdir("./target/assets/textures");
    }
    compile_asset("./assets/textures/cobbled_stone.png", "./target/assets/textures/cobbled_stone.h");
    compile_asset("./assets/textures/grass.png", "./target/assets/textures/grass.h");
    compile_asset("./assets/textures/dirt.png", "./target/assets/textures/dirt.h");
    compile_asset("./assets/textures/grass_side.png", "./target/assets/textures/grass_side.h");
}

int main() {
    if(!Build.fs.exists("./target")) {
        Build.fs.mkdir("./target");
    }
    make_assets();
    Build.fetch_git("https://github.com/glfw/glfw.git", false);
    compile_cmake("glfw", "GLFW");
    Build.fetch_git("https://github.com/recp/cglm.git", false);
    compile_cmake("cglm", "cglm");
    Build.fetch_git("https://github.com/nothings/stb.git", false);
    Build.build(
            OBJECT("./target/glad"), 
            StringArray("./glad/src/gl.c"), 
            1, 
            FlagArray(
                FLAG_COMPILE_ONLY, 
                FLAG_INCLUDE_PATH("./glad/include/")
            ),
            2
            );
    Build.build(
            OBJECT("./target/main"), 
            StringArray(
                "./main.c", 
                "./target/assets/shaders/frag.h", 
                "./target/assets/shaders/vert.h", 
                "./target/assets/shaders/geo.h",
                "./target/assets/textures/cobbled_stone.h",
                "./target/assets/textures/grass.h", 
                "./target/assets/textures/dirt.h",
                "./target/assets/textures/grass_side.h"
                ), 
            8, 
            FlagArray(
                FLAG_COMPILE_ONLY, 
                FLAG_INCLUDE_PATH("./deps/glfw/include/"), 
                FLAG_INCLUDE_PATH("./glad/include/"), 
                FLAG_INCLUDE_PATH("./target/"),
                FLAG_INCLUDE_PATH("./deps/cglm/include/"),
                FLAG_INCLUDE_PATH("./deps/stb"),
                ),
            6);
    Build.build(
            EXECUTABLE("./main"),
            StringArray(OBJECT("./target/main"), "./deps/glfw/build/src/libglfw3"LIB, OBJECT("./target/glad"), "./deps/cglm/build/libcglm"LIB),
            4,
            PLATFORM_LIBS
            );
    return 0;
}

