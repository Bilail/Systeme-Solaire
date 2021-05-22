//SDL Libraries
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_image.h>
#include <stack>
#include <cmath>
#include <vector>
//OpenGL Libraries
#include <GL/glew.h>
#include <GL/gl.h>

//GML libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"


#include "logger.h"

#include "Cube.h"
#include "Sphere.h"

#define vPositions 0
#define vNormals 1
#define vUV 2

#define WIDTH     1000
#define HEIGHT    1000
#define FRAMERATE 60
#define TIME_PER_FRAME_MS  (1.0f/FRAMERATE * 1e3)
#define INDICE_TO_PTR(x) ((void*)(x))

struct Objet
{
    GLint VAO;
    int   nbVertices;
    glm::mat4 propagatedMatrix = glm::mat4(1.0f);
    glm::mat4 localMatrix = glm::mat4(1.0f);
    float angle = 0.0f;
    std::vector<Objet*> children;
    glm::vec3 Color;
    glm::vec3 Constants = glm::vec3(0.2, 0.5, 0.4);
    float Alpha = 50.0;
    GLuint texture;
};

struct Light {

    glm::vec3 lightPosition = glm::vec3(0.0, 0.0, 0.0);
    glm::vec3 lightColor = glm::vec3(1.0, 1.0, 1.0);
};

void Draw(std::stack<glm::mat4>& modelstack, const glm::mat4& view, const glm::mat4& projection, Shader* shader, Objet& objet, Light& light)
{
    glUseProgram(shader->getProgramID());
    glBindVertexArray(objet.VAO);

    modelstack.push(modelstack.top() * objet.propagatedMatrix);

    glm::mat4 modelMatrix = modelstack.top() * objet.localMatrix;
    glm::mat3 inv_modelMatrix = glm::inverse(glm::mat3(modelMatrix));

    glm::mat4 mvp = projection * glm::inverse(view) * modelMatrix;

    glm::vec4 tmp = glm::inverse(projection * glm::inverse(view)) * glm::vec4(0, 0, -1, 1);
    glm::vec3 camera = glm::vec3(tmp) / tmp.w;

    GLint uMVP = glGetUniformLocation(shader->getProgramID(), "uMVP");
    glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvp));

    GLint color = glGetUniformLocation(shader->getProgramID(), "color");
    glUniform3fv(color, 1, glm::value_ptr(objet.Color));

    GLint constants = glGetUniformLocation(shader->getProgramID(), "constants");
    glUniform3fv(constants, 1, glm::value_ptr(objet.Constants));

    GLint alpha = glGetUniformLocation(shader->getProgramID(), "alpha");
    glUniform1f(alpha, objet.Alpha);

    GLint lightcolor = glGetUniformLocation(shader->getProgramID(), "lightcolor");
    glUniform3fv(lightcolor, 1, glm::value_ptr(light.lightColor));

    GLint lightposition = glGetUniformLocation(shader->getProgramID(), "lightposition");
    glUniform3fv(lightposition, 1, glm::value_ptr(light.lightPosition));

    GLint modelmatrix = glGetUniformLocation(shader->getProgramID(), "modelmatrix");
    glUniformMatrix4fv(modelmatrix, 1, GL_FALSE, glm::value_ptr(modelMatrix));

    GLint inv_modelmatrix = glGetUniformLocation(shader->getProgramID(), "inv_modelmatrix");
    glUniformMatrix3fv(inv_modelmatrix, 1, GL_FALSE, glm::value_ptr(inv_modelMatrix));

    GLint cameraposition = glGetUniformLocation(shader->getProgramID(), "cameraposition");
    glUniform3fv(cameraposition, 1, glm::value_ptr(camera));

    //uTexture
    GLint uTexture = glGetUniformLocation(shader->getProgramID(), "uTexture");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, objet.texture); //Ici je dis que GL_TEXTURE_0 est la texture de l'objet
    glUniform1i(uTexture, 0); //uTexture vaut ici ce que vaut GL_TEXTURE_0.

    glDrawArrays(GL_TRIANGLES, 0, objet.nbVertices); //Je d�ssine l'objet car uTexture vaut ce que vaut GL_TEXTURE_0, et que GL_TEXTURE_0 == objet.texture


    for (Objet* child : objet.children) {
        Draw(modelstack, view, projection, shader, *child, light);
    }

    modelstack.pop();

    glBindVertexArray(0);
    glUseProgram(0);
}


GLuint generate_VAO(const Geometry& forme)
{
    GLuint VBO;
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, (3 + 3 + 2) * sizeof(float) * forme.getNbVertices(), NULL, GL_DYNAMIC_DRAW);

    glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(float) * forme.getNbVertices(), forme.getVertices());
    glBufferSubData(GL_ARRAY_BUFFER, 3 * sizeof(float) * forme.getNbVertices(), 3 * sizeof(float) * forme.getNbVertices(), forme.getNormals());
    glBufferSubData(GL_ARRAY_BUFFER, (3 + 3) * sizeof(float) * forme.getNbVertices(), forme.getNbVertices() * 2 * sizeof(float), forme.getUVs());

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glVertexAttribPointer(vPositions, 3, GL_FLOAT, 0, 0, 0);
    glEnableVertexAttribArray(vPositions);


    glVertexAttribPointer(vNormals, 3, GL_FLOAT, 0, 0, INDICE_TO_PTR(3 * forme.getNbVertices() * sizeof(float)));
    glEnableVertexAttribArray(vNormals);

    glVertexAttribPointer(vUV, 2, GL_FLOAT, 0, 0, INDICE_TO_PTR((3 + 3) * forme.getNbVertices() * sizeof(float)));
    glEnableVertexAttribArray(vUV);

    return vao;
}

int main(int argc, char* argv[])
{
    ////////////////////////////////////////
    //SDL2 / OpenGL Context initialization : 
    ////////////////////////////////////////

    //Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
    {
        ERROR("The initialization of the SDL failed : %s\n", SDL_GetError());
        return 0;
    }

    //Create a Window
    SDL_Window* window = SDL_CreateWindow("Systeme Solaire",                           //Titre
        SDL_WINDOWPOS_UNDEFINED,               //X Position
        SDL_WINDOWPOS_UNDEFINED,               //Y Position
        WIDTH, HEIGHT,                         //Resolution
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN); //Flags (OpenGL + Show)

//Initialize OpenGL Version (version 3.0)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    //Initialize the OpenGL Context (where OpenGL resources (Graphics card resources) lives)
    SDL_GLContext context = SDL_GL_CreateContext(window);



    //Tells GLEW to initialize the OpenGL function with this version
    glewExperimental = GL_TRUE;
    glewInit();


    //Start using OpenGL to draw something on screen
    glViewport(0, 0, WIDTH, HEIGHT); //Draw on ALL the screen

    //The OpenGL background color (RGBA, each component between 0.0f and 1.0f)
    glClearColor(0.0, 0.0, 0.0, 1.0); //Full Black

    glEnable(GL_DEPTH_TEST); //Active the depth test

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        ERROR("Could not load SDL2_image with PNG files\n");
        return EXIT_FAILURE;
    }

    // Lire Texture CPU
    SDL_Surface* img_soleil = IMG_Load("../Textures/2k_sun.png");
    SDL_Surface* rgbImg_soleil = SDL_ConvertSurfaceFormat(img_soleil, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_soleil);

    SDL_Surface* img_terre = IMG_Load("../Textures/8k_earth_daymap.png");
    SDL_Surface* rgbImg_terre = SDL_ConvertSurfaceFormat(img_terre, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_terre);

    SDL_Surface* img_lune = IMG_Load("../Textures/8k_moon.png");
    SDL_Surface* rgbImg_lune = SDL_ConvertSurfaceFormat(img_lune, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_lune);

    SDL_Surface* img_mercury = IMG_Load("../Textures/2k_mercury.png");
    SDL_Surface* rgbImg_mercury = SDL_ConvertSurfaceFormat(img_mercury, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_mercury);

    // POSSIBLE DE SUPERPOSEE 2 TEXTURES A VOIR..
    SDL_Surface* img_venus = IMG_Load("../Textures/2k_venus_surface.png");
    SDL_Surface* rgbImg_venus = SDL_ConvertSurfaceFormat(img_venus, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_venus);

    SDL_Surface* img_jupiter = IMG_Load("../Textures/2k_jupiter.png");
    SDL_Surface* rgbImg_jupiter = SDL_ConvertSurfaceFormat(img_jupiter, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_jupiter);

    //POSSIBLE DE SUPERPOSEE 2 TEXTURES A VOIR..
    SDL_Surface* img_saturne = IMG_Load("../Textures/2k_saturn.png");
    SDL_Surface* rgbImg_saturne = SDL_ConvertSurfaceFormat(img_saturne, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_saturne);

    SDL_Surface* img_uranus = IMG_Load("../Textures/2k_uranus.png");
    SDL_Surface* rgbImg_uranus = SDL_ConvertSurfaceFormat(img_uranus, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_uranus);

    SDL_Surface* img_neptune = IMG_Load("../Textures/2k_neptune.png");
    SDL_Surface* rgbImg_neptune = SDL_ConvertSurfaceFormat(img_neptune, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_neptune);

    SDL_Surface* img_mars = IMG_Load("../Textures/2k_mars.png");
    SDL_Surface* rgbImg_mars = SDL_ConvertSurfaceFormat(img_mars, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img_mars);




    // Cr�ation des plan�tes, g�n�ration VBO, Bind Texture
    Sphere sphere(32, 32);


    Objet lune;
    lune.VAO = generate_VAO(sphere);
    lune.nbVertices = sphere.getNbVertices();
    lune.Color = glm::vec3(0.5, 0.5, 0.5);
    lune.propagatedMatrix = glm::scale(lune.propagatedMatrix, glm::vec3(0.1, 0.1, 0.1));
    lune.propagatedMatrix = glm::translate(lune.propagatedMatrix, glm::vec3(8.0, 0.0, 0.0));
    glGenTextures(1, &lune.texture);
    glBindTexture(GL_TEXTURE_2D, lune.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_lune->w, rgbImg_lune->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_lune->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet mercury;
    mercury.VAO = generate_VAO(sphere);
    mercury.nbVertices = sphere.getNbVertices();
    mercury.Color = glm::vec3(0.0, 0.0, 1.0);
    mercury.propagatedMatrix = glm::scale(mercury.propagatedMatrix, glm::vec3(0.3, 0.3, 0.3));
    mercury.propagatedMatrix = glm::translate(mercury.propagatedMatrix, glm::vec3(3.0, 0.0, 0.0));
    glGenTextures(1, &mercury.texture);
    glBindTexture(GL_TEXTURE_2D, mercury.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_mercury->w, rgbImg_mercury->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_mercury->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet dad_mercury;
    dad_mercury.VAO = generate_VAO(sphere);
    dad_mercury.nbVertices = sphere.getNbVertices();
    dad_mercury.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_mercury.propagatedMatrix = glm::scale(dad_mercury.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_mercury.children = { &mercury };


    Objet venus;
    venus.VAO = generate_VAO(sphere);
    venus.nbVertices = sphere.getNbVertices();
    venus.Color = glm::vec3(0.0, 0.0, 1.0);
    venus.propagatedMatrix = glm::scale(venus.propagatedMatrix, glm::vec3(0.9, 0.9, 0.9));
    venus.propagatedMatrix = glm::translate(venus.propagatedMatrix, glm::vec3(1.95, 0.0, 0.0));
    glGenTextures(1, &venus.texture);
    glBindTexture(GL_TEXTURE_2D, venus.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_venus->w, rgbImg_venus->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_venus->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet dad_venus;
    dad_venus.VAO = generate_VAO(sphere);
    dad_venus.nbVertices = sphere.getNbVertices();
    dad_venus.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_venus.propagatedMatrix = glm::scale(dad_venus.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_venus.children = { &venus };


    Objet terre;
    terre.VAO = generate_VAO(sphere);
    terre.nbVertices = sphere.getNbVertices();
    terre.Color = glm::vec3(0.0, 0.0, 1.0);
    terre.propagatedMatrix = glm::scale(terre.propagatedMatrix, glm::vec3(0.9, 0.9, 0.9));
    terre.propagatedMatrix = glm::translate(terre.propagatedMatrix, glm::vec3(3.5, 0.0, 0.0));
    terre.children = { &lune };
    glGenTextures(1, &terre.texture);
    glBindTexture(GL_TEXTURE_2D, terre.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_terre->w, rgbImg_terre->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_terre->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet dad_terre;
    dad_terre.VAO = generate_VAO(sphere);
    dad_terre.nbVertices = sphere.getNbVertices();
    terre.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_terre.propagatedMatrix = glm::scale(dad_terre.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_terre.children = { &terre };

    Objet mars;
    mars.VAO = generate_VAO(sphere);
    mars.nbVertices = sphere.getNbVertices();
    mars.Color = glm::vec3(0.0, 0.0, 1.0);
    mars.propagatedMatrix = glm::scale(mars.propagatedMatrix, glm::vec3(0.4, 0.4, 0.4));
    mars.propagatedMatrix = glm::translate(mars.propagatedMatrix, glm::vec3(10.5, 0.0, 0.0));
    glGenTextures(1, &mars.texture);
    glBindTexture(GL_TEXTURE_2D, mars.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_mars->w, rgbImg_mars->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_mars->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet dad_mars;
    dad_mars.VAO = generate_VAO(sphere);
    dad_mars.nbVertices = sphere.getNbVertices();
    dad_mars.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_mars.propagatedMatrix = glm::scale(dad_mars.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_mars.children = { &mars };

    Objet jupiter;
    jupiter.VAO = generate_VAO(sphere);
    jupiter.nbVertices = sphere.getNbVertices();
    jupiter.Color = glm::vec3(0.0, 0.0, 1.0);
    jupiter.propagatedMatrix = glm::scale(jupiter.propagatedMatrix, glm::vec3(2.0, 2.0, 2.0));
    jupiter.propagatedMatrix = glm::translate(jupiter.propagatedMatrix, glm::vec3(3.0, 0.0, 0.0));
    glGenTextures(1, &jupiter.texture);
    glBindTexture(GL_TEXTURE_2D, jupiter.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_jupiter->w, rgbImg_jupiter->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_jupiter->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet dad_jupiter;
    dad_jupiter.VAO = generate_VAO(sphere);
    dad_jupiter.nbVertices = sphere.getNbVertices();
    dad_jupiter.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_jupiter.propagatedMatrix = glm::scale(dad_jupiter.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_jupiter.children = { &jupiter };

    Objet saturne;
    saturne.VAO = generate_VAO(sphere);
    saturne.nbVertices = sphere.getNbVertices();
    saturne.Color = glm::vec3(0.0, 0.0, 1.0);
    saturne.propagatedMatrix = glm::scale(saturne.propagatedMatrix, glm::vec3(1.7, 1.7, 1.7));
    saturne.propagatedMatrix = glm::translate(saturne.propagatedMatrix, glm::vec3(5.0, 0.0, 0.0));
    glGenTextures(1, &saturne.texture);
    glBindTexture(GL_TEXTURE_2D, saturne.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_saturne->w, rgbImg_saturne->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_saturne->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }

    Objet dad_saturne;
    dad_saturne.VAO = generate_VAO(sphere);
    dad_saturne.nbVertices = sphere.getNbVertices();
    dad_saturne.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_saturne.propagatedMatrix = glm::scale(dad_saturne.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_saturne.children = { &saturne };

    Objet uranus;
    uranus.VAO = generate_VAO(sphere);
    uranus.nbVertices = sphere.getNbVertices();
    uranus.Color = glm::vec3(0.0, 0.0, 1.0);
    uranus.propagatedMatrix = glm::scale(uranus.propagatedMatrix, glm::vec3(1.2, 1.2, 1.2));
    uranus.propagatedMatrix = glm::translate(uranus.propagatedMatrix, glm::vec3(9.0, 0.0, 0.0));
    glGenTextures(1, &uranus.texture);
    glBindTexture(GL_TEXTURE_2D, uranus.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_uranus->w, rgbImg_uranus->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_uranus->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet dad_uranus;
    dad_uranus.VAO = generate_VAO(sphere);
    dad_uranus.nbVertices = sphere.getNbVertices();
    dad_uranus.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_uranus.propagatedMatrix = glm::scale(dad_uranus.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_uranus.children = { &uranus };


    Objet neptune;
    neptune.VAO = generate_VAO(sphere);
    neptune.nbVertices = sphere.getNbVertices();
    neptune.Color = glm::vec3(0.0, 0.0, 1.0);
    neptune.propagatedMatrix = glm::scale(neptune.propagatedMatrix, glm::vec3(1.2, 1.2, 1.2));
    neptune.propagatedMatrix = glm::translate(neptune.propagatedMatrix, glm::vec3(11.0, 0.0, 0.0));
    glGenTextures(1, &neptune.texture);
    glBindTexture(GL_TEXTURE_2D, neptune.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_neptune->w, rgbImg_neptune->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_neptune->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Objet dad_neptune;
    dad_neptune.VAO = generate_VAO(sphere);
    dad_neptune.nbVertices = sphere.getNbVertices();
    dad_neptune.Color = glm::vec3(1.0, 1.0, 0.0);
    dad_neptune.propagatedMatrix = glm::scale(dad_neptune.propagatedMatrix, glm::vec3(4.9, 4.9, 4.9));
    dad_neptune.children = { &neptune };

    Objet soleil;
    soleil.VAO = generate_VAO(sphere);
    soleil.nbVertices = sphere.getNbVertices();
    soleil.propagatedMatrix = glm::scale(soleil.propagatedMatrix, glm::vec3(5.0, 5.0, 5.0));

    glGenTextures(1, &soleil.texture);
    glBindTexture(GL_TEXTURE_2D, soleil.texture);
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg_soleil->w, rgbImg_soleil->h, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)rgbImg_soleil->pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    //From here you can load your OpenGL objects, like VBO, Shaders, etc.

    FILE* vertFile = fopen("Shaders/color.vert", "r");
    FILE* fragFile = fopen("Shaders/color.frag", "r");

    Shader* shader = Shader::loadFromFiles(vertFile, fragFile);

    fclose(vertFile);
    fclose(fragFile);

    if (shader == NULL)
    {
        return EXIT_FAILURE;
    }


    bool isOpened = true;
    float t = 0.0f;
    int compteur = 0;
    float angle = 0.0f;
    bool camdep = false;
    bool gauche = false;
    bool droite = false;
    bool haut = false;
    bool bas = false;
    bool zooma = false;
    bool zoomb = false;
    float camx = 0.0f;
    float camy = 0.0f;
    float camz = 5.0f;
    bool reset = false;
    float vitesse_dep = 0.005f;
    float vitesse_transi = 0.5f;
    float vitesse_zoom = 0.05f;

    //Main application loop
    while (isOpened)
    {

        // -------- ANIMATION PLANETE --------
       // LUNE --------
        lune.localMatrix = glm::rotate(lune.localMatrix, glm::radians(10.0f), glm::vec3(1.0, 1.0, 1.0));

        // Les dad_PLANETE
        dad_terre.propagatedMatrix = glm::rotate(dad_terre.propagatedMatrix, glm::radians(0.62f), glm::vec3(0.0, 1.0, 0.0));
        dad_mercury.propagatedMatrix = glm::rotate(dad_mercury.propagatedMatrix, glm::radians(1.0f), glm::vec3(0.0, 1.0, 0.0));
        dad_venus.propagatedMatrix = glm::rotate(dad_venus.propagatedMatrix, glm::radians(0.73f), glm::vec3(0.0, 1.0, 0.0));
        dad_mars.propagatedMatrix = glm::rotate(dad_mars.propagatedMatrix, glm::radians(0.5f), glm::vec3(0.0, 1.0, 0.0));
        dad_jupiter.propagatedMatrix = glm::rotate(dad_jupiter.propagatedMatrix, glm::radians(0.27f), glm::vec3(0.0, 1.0, 0.0));
        dad_saturne.propagatedMatrix = glm::rotate(dad_saturne.propagatedMatrix, glm::radians(0.20f), glm::vec3(0.0, 1.0, 0.0));
        dad_uranus.propagatedMatrix = glm::rotate(dad_uranus.propagatedMatrix, glm::radians(0.14f), glm::vec3(0.0, 1.0, 0.0));
        dad_neptune.propagatedMatrix = glm::rotate(dad_neptune.propagatedMatrix, glm::radians(0.11f), glm::vec3(0.0, 1.0, 0.0));
        terre.propagatedMatrix = glm::rotate(terre.propagatedMatrix, glm::radians(1.62f), glm::vec3(0.0, 1.0, 0.0));

        lune.localMatrix = glm::rotate(lune.localMatrix, glm::radians(0.041f), glm::vec3(1.0, 1.0, 1.0));
        terre.localMatrix = glm::rotate(terre.localMatrix, glm::radians(0.041f), glm::vec3(1.0, 1.0, 1.0));
        soleil.localMatrix = glm::rotate(soleil.localMatrix, glm::radians(1.02f), glm::vec3(1.0, 1.0, 1.0));
        mercury.localMatrix = glm::rotate(mercury.localMatrix, glm::radians(2.386f), glm::vec3(1.0, 1.0, 1.0));
        venus.localMatrix = glm::rotate(venus.localMatrix, glm::radians(10.0f), glm::vec3(1.0, 1.0, 1.0));
        mars.localMatrix = glm::rotate(mars.localMatrix, glm::radians(0.041f), glm::vec3(1.0, 1.0, 1.0));
        jupiter.localMatrix = glm::rotate(jupiter.localMatrix, glm::radians(0.015f), glm::vec3(1.0, 1.0, 1.0));
        saturne.localMatrix = glm::rotate(saturne.localMatrix, glm::radians(0.017f), glm::vec3(1.0, 1.0, 1.0));
        uranus.localMatrix = glm::rotate(uranus.localMatrix, glm::radians(0.0257f), glm::vec3(1.0, 1.0, 1.0));
        neptune.localMatrix = glm::rotate(neptune.localMatrix, glm::radians(0.030f), glm::vec3(1.0, 1.0, 1.0));


        glm::mat4 View(1.0f);


        //Time in ms telling us when this frame started. Useful for keeping a fix framerate
        uint32_t timeBegin = SDL_GetTicks();

        //Fetch the SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {

            switch (event.type)
            {
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_CLOSE:
                    isOpened = false;
                    break;
                default:
                    break;
                }
                break;
                //We can add more event, like listening for the keyboard or the mouse. See SDL_Event documentation for more details
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                case SDLK_SPACE:
                    if (camdep == false)
                    {
                        camdep = true;
                    }
                    else
                    {
                        camdep = false;
                    }
                    break;
                case SDLK_LEFT:
                    gauche = true;
                    break;
                case SDLK_RIGHT:
                    droite = true;
                    break;
                case SDLK_UP:
                    haut = true;
                    break;
                case SDLK_DOWN:
                    bas = true;
                    break;
                case SDLK_r:
                    reset = true;
                    break;
                case SDLK_z:
                    zooma = true;
                    break;
                case SDLK_s:
                    zoomb = true;
                    break;
                default:
                    break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym)
                {
                case SDLK_LEFT:
                    gauche = false;
                    break;
                case SDLK_RIGHT:
                    droite = false;
                    break;
                case SDLK_UP:
                    haut = false;
                    break;
                case SDLK_DOWN:
                    bas = false;
                    break;
                case SDLK_r:
                    reset = false;
                    break;
                case SDLK_z:
                    zooma = false;
                    break;
                case SDLK_s:
                    zoomb = false;
                    break;
                default:
                    break;

                }
            }
        }

        //Clear the screen : the depth buffer and the color buffer
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        if (camdep)
        {
            if (angle <= 90.0f)
            {
                View = glm::rotate(View, glm::radians(angle), glm::vec3(1.0, 0.0, 0.0));
                angle = angle + vitesse_transi;
            }
            else
            {
                View = glm::rotate(View, glm::radians(90.0f), glm::vec3(1.0, 0.0, 0.0));
            }
            if (gauche)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camx = camx - vitesse_dep;
            }
            if (droite)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camx = camx + vitesse_dep;
            }
            if (haut)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camy = camy + vitesse_dep;
            }
            if (bas)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camy = camy - vitesse_dep;
            }
            if (gauche == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (droite == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (haut == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (bas == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (reset)
            {
                camx = 0.0f;
                camy = 0.0f;
                camz = 5.0f;
            }
            if (zooma)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camz = camz - vitesse_zoom;
            }
            if (zoomb)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camz = camz + vitesse_zoom;
            }
            if (zooma == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (zoomb == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
        }

        if (camdep == false)
        {
            if (angle >= 0.0f)
            {
                View = glm::rotate(View, glm::radians(angle), glm::vec3(1.0, 0.0, 0.0));
                angle = angle - vitesse_transi;
            }
            else
            {
                View = glm::rotate(View, glm::radians(0.0f), glm::vec3(1.0, 0.0, 0.0));
            }
            if (gauche)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camx = camx - vitesse_dep;
            }
            if (droite)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camx = camx + vitesse_dep;
            }
            if (haut)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camy = camy + vitesse_dep;
            }
            if (bas)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camy = camy - vitesse_dep;
            }
            if (gauche == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (droite == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (haut == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (bas == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (reset)
            {
                camx = 0.0f;
                camy = 0.0f;
                camz = 5.0f;
            }
            if (zooma)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camz = camz - vitesse_zoom;
            }
            if (zoomb)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
                camz = camz + vitesse_zoom;
            }
            if (zooma == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
            if (zoomb == false)
            {
                View = glm::translate(View, glm::vec3(camx, camy, camz));
            }
        }


        glm::mat4 Projection(1.0f);
        Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
        std::stack<glm::mat4> modelstack;
        modelstack.push(glm::mat4(1.0f));
        Light light;


        Draw(modelstack, View, Projection, shader, soleil, light);
        Draw(modelstack, View, Projection, shader, dad_mercury, light);
        Draw(modelstack, View, Projection, shader, dad_terre, light);
        Draw(modelstack, View, Projection, shader, dad_venus, light);
        Draw(modelstack, View, Projection, shader, dad_mars, light);
        Draw(modelstack, View, Projection, shader, dad_jupiter, light);
        Draw(modelstack, View, Projection, shader, dad_saturne, light);
        Draw(modelstack, View, Projection, shader, dad_uranus, light);
        Draw(modelstack, View, Projection, shader, dad_neptune, light);


        //Display on screen (swap the buffer on screen and the buffer you are drawing on)
        SDL_GL_SwapWindow(window);

        //Time in ms telling us when this frame ended. Useful for keeping a fix framerate
        uint32_t timeEnd = SDL_GetTicks();

        //We want FRAMERATE FPS
        if (timeEnd - timeBegin < TIME_PER_FRAME_MS)
            SDL_Delay(TIME_PER_FRAME_MS - (timeEnd - timeBegin));

    }

    //Free everything
    if (context != NULL)
        SDL_GL_DeleteContext(context);
    if (window != NULL)
        SDL_DestroyWindow(window);

    return 0;

    SDL_FreeSurface(rgbImg_soleil);
    SDL_FreeSurface(rgbImg_mercury);
    SDL_FreeSurface(rgbImg_venus);
    SDL_FreeSurface(rgbImg_terre);
    SDL_FreeSurface(rgbImg_lune);
    SDL_FreeSurface(rgbImg_mars);
    SDL_FreeSurface(rgbImg_jupiter);
    SDL_FreeSurface(rgbImg_saturne);
    SDL_FreeSurface(rgbImg_uranus);
    SDL_FreeSurface(rgbImg_neptune);

}
