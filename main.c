#include <stdlib.h>
#include <stdio.h>

#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <assets/shaders/vert.h>
#include <assets/shaders/frag.h>
#include <assets/textures/cobbled_stone.h>
#include <assets/shaders/geo.h>
#include <assets/textures/grass.h>
#include <string.h>
#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

char* len_to_cstr(unsigned char* str, unsigned int len) {
    unsigned char* new = malloc(len + 1);
    if (!new) return NULL;
    memcpy(new, str, len);
    new[len] = '\0';
    return (char*) new;
}

GLuint compile_shader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "ERROR: Shader compilation failed:\n%s\n", infoLog);
        return 0;
    }
    return shader;
}

GLuint create_shader_program(const char* vert_src, const char* frag_src, const char* geo_shader) {
    GLuint vertex = compile_shader(vert_src, GL_VERTEX_SHADER);
    if (vertex == 0) return 0;
    GLuint fragment = compile_shader(frag_src, GL_FRAGMENT_SHADER);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return 0;
    }
    GLuint geo = compile_shader(geo_shader, GL_GEOMETRY_SHADER);
    if (fragment == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glAttachShader(program, geo);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        fprintf(stderr, "ERROR: Program linking failed:\n%s\n", infoLog);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteShader(geo);
        return 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glDeleteShader(geo);

    return program;
}

int WIDTH, HEIGHT;
vec3 pos = {0, 0, 0};
float yaw = -90.0f; // Start facing -Z
float pitch = 0.0f;
float lastX;
float lastY;
bool firstMouse = true;
vec3 front = {0.0f, 0.0f, -1.0f};
vec3 up = {0.0f, 1.0f, 0.0f};

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed: y goes up
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // clamp pitch to prevent screen flip
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // convert to direction vector
    front[0] = cosf(glm_rad(yaw)) * cosf(glm_rad(pitch));
    front[1] = sinf(glm_rad(pitch));
    front[2] = sinf(glm_rad(yaw)) * cosf(glm_rad(pitch));
    glm_normalize(front);
}


void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

bool isKeyHeld(GLFWwindow* window, int key) {
    int state = glfwGetKey(window, key);
    return state == GLFW_PRESS;
}

#define SPEED 0.1f

void update(GLFWwindow* window) {
    vec3 move = {0, 0, 0};

    vec3 forward = { cosf(glm_rad(yaw)), 0.0f, sinf(glm_rad(yaw)) };
    vec3 right;
    glm_vec3_crossn((vec3){0.0f, 1.0f, 0.0f}, forward, right); // Y-up cross forward = right

    if (isKeyHeld(window, GLFW_KEY_W)) glm_vec3_add(move, forward, move);
    if (isKeyHeld(window, GLFW_KEY_S)) glm_vec3_sub(move, forward, move);
    if (isKeyHeld(window, GLFW_KEY_D)) glm_vec3_sub(move, right, move);
    if (isKeyHeld(window, GLFW_KEY_A)) glm_vec3_add(move, right, move);

    if (glm_vec3_norm(move) > 0.0f)
        glm_normalize(move);

    glm_vec3_scale(move, SPEED, move);
    if (isKeyHeld(window, GLFW_KEY_SPACE)) move[1]+=SPEED; 
    if (isKeyHeld(window, GLFW_KEY_LEFT_SHIFT)) move[1]-=SPEED;
    glm_vec3_add(pos, move, pos);
}

void set_size(int width, int height) {
    WIDTH = width;
    HEIGHT = height;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    set_size(width, height);
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    glUniform2f(glGetUniformLocation(currentProgram, "screenSize"), (float)WIDTH, (float)HEIGHT);
}


typedef struct {
    float umin, umax, vmin, vmax;
} UV;

typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;  // keep channels per image in case it varies (or assume all same)
    UV* uv;
} Img;


unsigned char* generate_texture_atlas_struct(
    Img* images,
    int count,
    int channels,         // assume all images have this channels count
    int* out_width,
    int* out_height,
    int* out_channels
    ) {
    int images_per_row = (int)ceilf(sqrtf(count));

    if (!images || count <= 0 || channels <= 0 || images_per_row <= 0) return NULL;

    // Calculate max width and height per row to handle varying sizes
    int atlas_width = 0;
    int atlas_height = 0;

    int row_width = 0;
    int row_height = 0;

    // Pre-calc atlas size (simple grid packing, no fancy bin-packing)
    int rows = (count + images_per_row - 1) / images_per_row;
    int* row_heights = malloc(rows * sizeof(int));
    if (!row_heights) return NULL;

    for (int r = 0; r < rows; r++) row_heights[r] = 0;

    // Track widths & heights of each row
    for (int i = 0; i < count; i++) {
        int col = i % images_per_row;
        int row = i / images_per_row;

        row_width += images[i].width;
        if (images[i].height > row_heights[row]) {
            row_heights[row] = images[i].height;
        }

        if (col == images_per_row - 1 || i == count -1) {
            if (row_width > atlas_width) atlas_width = row_width;
            row_width = 0;
        }
    }

    for (int r = 0; r < rows; r++) {
        atlas_height += row_heights[r];
    }

    unsigned char* atlas = malloc(atlas_width * atlas_height * channels);
    if (!atlas) {
        free(row_heights);
        return NULL;
    }
    memset(atlas, 0, atlas_width * atlas_height * channels);

    int y_offset = 0;
    for (int r = 0; r < rows; r++) {
        int x_offset = 0;
        for (int c = 0; c < images_per_row; c++) {
            int i = r * images_per_row + c;
            if (i >= count) break;

            Img* img = &images[i];
            for (int y = 0; y < img->height; y++) {
                unsigned char* dst = atlas + ((y_offset + y) * atlas_width + x_offset) * channels;
                unsigned char* src = img->data + y * img->width * channels;
                memcpy(dst, src, img->width * channels);
            }
            if (img->uv) {
                img->uv->umin = (float)x_offset / atlas_width;
                img->uv->umax = (float)(x_offset + img->width) / atlas_width;
                img->uv->vmin = (float)(y_offset + img->height) / atlas_height; // OpenGL UV origin = bottom left
                img->uv->vmax = (float)(y_offset) / atlas_height;
            }
            x_offset += img->width;
        }
        y_offset += row_heights[r];
    }

    free(row_heights);

    *out_width = atlas_width;
    *out_height = atlas_height;
    *out_channels = channels;

    return atlas;
}

Img load_image(unsigned char*data, int len, int desired, UV* uv) {
    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(data, len, &width, &height, &channels, desired);
    return(Img) {
        .data=pixels,
        .height=height,
        .width=width,
        .channels=channels,
        .uv = uv
    };
}

struct {
    UV Cobbled_Stone;
    UV Grass;
} Textures = {};

unsigned char* get_atlas(int* out_width, int*out_height, int* out_channels, int desired) {
    Img cobbled_stone = load_image(__assets_textures_cobbled_stone_png, __assets_textures_cobbled_stone_png_len, desired, &Textures.Cobbled_Stone);
    Img grass = load_image(__assets_textures_grass_png, __assets_textures_grass_png_len, desired, &Textures.Grass);
    unsigned char* atlas =  generate_texture_atlas_struct((Img[]) {
            cobbled_stone,
            grass,
            }, 2, desired, out_width, out_height, out_channels);
    stbi_image_free(cobbled_stone.data);
    stbi_image_free(grass.data);
    return atlas;
}

int main(void) {
    char* vert_shader = len_to_cstr(__assets_shaders_vert_glsl, __assets_shaders_vert_glsl_len);
    char* frag_shader = len_to_cstr(__assets_shaders_frag_glsl, __assets_shaders_frag_glsl_len);
    char* geo_shader = len_to_cstr(__assets_shaders_geo_glsl, __assets_shaders_geo_glsl_len);
    int width, height, channels;
    unsigned char* pixels = get_atlas(&width, &height, &channels, 4);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    set_size(mode->width, mode->height);
    lastX = WIDTH / 2.0f;
    lastY = HEIGHT / 2.0f;
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "minceraft", primary, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


    int version = gladLoadGL(glfwGetProcAddress);
    if (!version) {
        fprintf(stderr, "Failed to init GLAD\n");
        glfwTerminate();
        return -1;
    }

    GLuint shaders = create_shader_program(vert_shader, frag_shader, geo_shader);
    free(vert_shader);
    free(frag_shader);
    free(geo_shader);
    if (!shaders) {
        fprintf(stderr, "Failed to create shader program\n");
        glfwTerminate();
        return -1;
    }
    UV text = Textures.Cobbled_Stone;
    float vertices[] = {
        // posX face
        0.5f,  0.5f, -0.5f,  text.umax, text.vmax,
        0.5f, -0.5f, -0.5f,  text.umax, text.vmin,
        0.5f, -0.5f,  0.5f,  text.umin, text.vmin,
        0.5f,  0.5f,  0.5f,  text.umin, text.vmax,

        // negX face
        -0.5f,  0.5f,  0.5f,  text.umax, text.vmax,
        -0.5f, -0.5f,  0.5f,  text.umax, text.vmin,
        -0.5f, -0.5f, -0.5f,  text.umin, text.vmin,
        -0.5f,  0.5f, -0.5f,  text.umin, text.vmax,

        // posY face
        -0.5f,  0.5f,  0.5f,  text.umin, text.vmax,
        0.5f,  0.5f,  0.5f,  text.umax, text.vmax,
        0.5f,  0.5f, -0.5f,  text.umax, text.vmin,
        -0.5f,  0.5f, -0.5f,  text.umin, text.vmin,

        // negY face
        -0.5f, -0.5f, -0.5f,  text.umin, text.vmax,
        0.5f, -0.5f, -0.5f,  text.umax, text.vmax,
        0.5f, -0.5f,  0.5f,  text.umax, text.vmin,
        -0.5f, -0.5f,  0.5f,  text.umin, text.vmin,

        // posZ face
        0.5f,  0.5f,  0.5f,  text.umax, text.vmax,
        0.5f, -0.5f,  0.5f,  text.umax, text.vmin,
        -0.5f, -0.5f,  0.5f,  text.umin, text.vmin,
        -0.5f,  0.5f,  0.5f,  text.umin, text.vmax,

        // negZ face
        -0.5f,  0.5f, -0.5f,  text.umax, text.vmax,
        -0.5f, -0.5f, -0.5f,  text.umax, text.vmin,
        0.5f, -0.5f, -0.5f,  text.umin, text.vmin,
        0.5f,  0.5f, -0.5f,  text.umin, text.vmax,
    };

    unsigned int indices[] = {
        2,1,0,
        3,2,0,
        6,5,4,
        7,6,4,
        8,9,10,
        8,10,11,
        12,13,14,
        12,14,15,
        18,17,16,
        19,18,16,
        22,21,20,
        23,22,20
    };
    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);      // Enable face culling


    // Setup projection matrix
    mat4 proj;
    float fov = glm_rad(45.0f);
    float near = 0.1f;
    float far = 100.0f;

    glUseProgram(shaders);
    GLuint projLoc = glGetUniformLocation(shaders, "projection");
    GLuint viewLoc = glGetUniformLocation(shaders, "view");
    glUniform2f(glGetUniformLocation(shaders, "screenSize"), (float)WIDTH, (float)HEIGHT);

    glEnable(GL_DEPTH_TEST);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    // Set filtering to nearest neighbor (sharp pixels)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    // Setup texture parameters (wrapping, filtering)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Upload pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glGenerateMipmap(GL_TEXTURE_2D);

    glUniform1i(glGetUniformLocation(shaders, "texture1"), 0);


    // free pixel data after uploading
    free(pixels);

    while (!glfwWindowShouldClose(window)) {
        float aspect = (float)WIDTH / (float)HEIGHT;
        glm_perspective(fov, aspect, near, far, proj);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, (const float*)proj);
        glfwPollEvents();
        update(window);
        mat4 view;
        vec3 target;
        glm_vec3_add(pos, front, target);
        glm_lookat(pos, target, up, view);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (const float*)view);


        glClearColor(0.0f, 0.56, 0.78f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (sizeof(indices)/sizeof(int)), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaders);

    glfwTerminate();

    return 0;
}

