#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include "verts.h"
#include "bones.h"
#include "shader.h"
#include "lodepng.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext.hpp>

#include <iostream>

using namespace std;

void initShader();
void initTexture();
void initBuffers();

GLuint loadTexture(const char* filename);
GLuint loadShader(GLenum type, const char* source);
GLuint initShaderProgram(const char*, const char*, GLuint*, GLuint*);

void calculateBoneMatrix();


float t = 0;
bool once = false;

GLuint positionBuffer, textureCoordBuffer, normalBuffer, tangentBuffer,
       boneIndexBuffer, boneWeightBuffer, indexBuffer1, indexBuffer2, indexBuffer3;

GLuint vertexShader, fragmentShader, shaderProgram;

GLint aVertexPosition, aTextureCoord, aNormal, aTangent, aBoneIndex, aBoneWeight,
      uProjectionMatrix, uModelViewMatrix, uBones, uEyes, uHat, uAoTexture,
      uNorTexture, uHatTexture, uFabricTexture;

GLuint aoTexture, norTexture, hatTexture, fabricTexture, settingsTexture;


void init() {
    initShader();
    initTexture();
    initBuffers();
}


void drawGui(float delta)
{
    float s = 2.0f;
    glLoadIdentity();
    glOrtho(0.0f, width*s, 0.0f, height*s, -1.0f, +1.0f);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, settingsTexture);
    glDisable(GL_DEPTH_TEST);


    glBegin(GL_QUADS);
        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex2i(0, 0);
        glVertex2i(width*2, 0);
        glVertex2i(width*2, 100);
        glVertex2i(0, 100);

        glTexCoord2f(0.0f, 1.0f);
        glVertex2i(0, 0);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2i(500, 0);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2i(500, 100);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 100);

        glColor3f(0.5f, 0.5f, 0.5f);

        glVertex2i(10, 12);
        glVertex2i(200*glm::pow(micThreshold, 0.25f)+6, 12);
        glVertex2i(200*glm::pow(micThreshold, 0.25f)+6, 38);
        glVertex2i(10, 38);

        glVertex2i(10, 64);
        glVertex2i(200*sensitivity+6, 64);
        glVertex2i(200*sensitivity+6, 90);
        glVertex2i(10, 90);

        if(displayBlink) {
            glVertex2i(375, 17);
            glVertex2i(391, 17);
            glVertex2i(391, 33);
            glVertex2i(375, 33);
        }

        if(displayHat) {
            glVertex2i(375, 69);
            glVertex2i(391, 69);
            glVertex2i(391, 85);
            glVertex2i(375, 85);
        }

        glColor3f(1.0f, 0.0f, 0.0f);

        glVertex2i(10, 16);
        glVertex2i(200*glm::pow(prevvol, 0.25f)+6, 16);
        glVertex2i(200*glm::pow(prevvol, 0.25f)+6, 34);
        glVertex2i(10, 34);
    glEnd();

    #define mouseIn(x1,y1,x2,y2) ((mouseX*s) >= x1 && (mouseX*s) <= x2 && ((height-mouseY)*s) >= y1 && ((height-mouseY)*s) <= y2)
    #define justPressed          (!prevMousePressed && mousePressed)

    if(mouseIn(10, 12, 206, 38)&& mousePressed) {
        micThreshold = glm::clamp((mouseX*s-10)/(206-10), 0.0f, 1.0f);
        micThreshold = glm::pow(micThreshold, 4.0f);
    }
    if(mouseIn(10, 64, 206, 90)&& mousePressed) {
        sensitivity = glm::clamp((mouseX*s-10)/(206-10), 0.0f, 1.0f);
    }

    if(mouseIn(370, 12, 400, 38)&& justPressed ) {
        displayBlink = displayBlink ? false : true;
    }
    if(mouseIn(370, 64, 400, 90) && justPressed) {
        displayHat = displayHat ? false : true;
    }
}

void loop(float delta) {
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClearDepth(1.0);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);


    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 50, width, height-50);


    float fieldOfView = 50 * 3.14152 / 180;
    float aspect = width / (height-50);
    float zNear = 0.1;
    float zFar = 100.0;

    float rotx = -1-0.4*glm::sin(t*0.6);
    float roty = 0.125*glm::sin(t*1.5);

    glm::mat4 projectionMatrix = glm::perspective(fieldOfView, aspect, zNear, zFar);
    glm::mat4 modelViewMatrix = glm::lookAt(
            glm::vec3(rotx, -7, 4+roty),
            glm::vec3(0, 0, 4),
            glm::vec3(0, 0, 1)
    );

    glUseProgram(shaderProgram);

    glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
    glVertexAttribPointer(aVertexPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aVertexPosition);

    glBindBuffer(GL_ARRAY_BUFFER, textureCoordBuffer);
    glVertexAttribPointer(aTextureCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aTextureCoord);

    glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
    glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aNormal);

    glBindBuffer(GL_ARRAY_BUFFER, tangentBuffer);
    glVertexAttribPointer(aTangent, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aTangent);

    glBindBuffer(GL_ARRAY_BUFFER, boneIndexBuffer);
    glVertexAttribPointer(aBoneIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aBoneIndex);

    glBindBuffer(GL_ARRAY_BUFFER, boneWeightBuffer);
    glVertexAttribPointer(aBoneWeight, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aBoneWeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, aoTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, norTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, hatTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, fabricTexture);
    glActiveTexture(GL_TEXTURE0);

    glUniform1i(uAoTexture, 0);
    glUniform1i(uNorTexture, 1);
    glUniform1i(uHatTexture, 2);
    glUniform1i(uFabricTexture, 3);

    glUniformMatrix4fv(uProjectionMatrix, 1, GL_FALSE, &projectionMatrix[0][0]);
    glUniformMatrix4fv(uModelViewMatrix, 1, GL_FALSE, &modelViewMatrix[0][0]);

    calculateBoneMatrix();

    // draw
    glUniform1i(uEyes, GL_FALSE);
    glUniform1i(uHat, GL_TRUE);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer3);
    if(displayHat)
        glDrawElements(GL_TRIANGLES, sizeof(t3)/sizeof(short), GL_UNSIGNED_SHORT, 0);

    glUniform1i(uHat, GL_FALSE);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer1);
    glDrawElements(GL_TRIANGLES, sizeof(t1)/sizeof(short), GL_UNSIGNED_SHORT, 0);

    glUniform1i(uEyes, GL_TRUE);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer2);
    glDrawElements(GL_TRIANGLES, sizeof(t2)/sizeof(short), GL_UNSIGNED_SHORT, 0);
    glUseProgram(0);

    glViewport(0, 0, width, height);
    if(mouseInWindow)
        drawGui(delta);

    t += delta;
    prevMousePressed = mousePressed;
}

void dispose() {
    glDeleteBuffers(1, &positionBuffer);
    glDeleteBuffers(1, &textureCoordBuffer);
    glDeleteBuffers(1, &normalBuffer);
    glDeleteBuffers(1, &tangentBuffer);
    glDeleteBuffers(1, &boneIndexBuffer);
    glDeleteBuffers(1, &boneWeightBuffer);
    glDeleteBuffers(1, &indexBuffer1);
    glDeleteBuffers(1, &indexBuffer2);
    glDeleteBuffers(1, &indexBuffer3);

    glDetachShader(shaderProgram, vertexShader);
    glDetachShader(shaderProgram, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(shaderProgram);

    glDeleteTextures(1, &aoTexture);
    glDeleteTextures(1, &norTexture);
    glDeleteTextures(1, &hatTexture);
    glDeleteTextures(1, &fabricTexture);
    glDeleteTextures(1, &settingsTexture);
}


void initShader() {
    shaderProgram = initShaderProgram(vsSource, fsSource, &vertexShader, &fragmentShader);

    aVertexPosition = glGetAttribLocation(shaderProgram, "aVertexPosition");
    aTextureCoord = glGetAttribLocation(shaderProgram, "aTextureCoord");
    aNormal = glGetAttribLocation(shaderProgram, "aNormal");
    aTangent = glGetAttribLocation(shaderProgram, "aTangent");
    aBoneIndex = glGetAttribLocation(shaderProgram, "aBoneIndex");
    aBoneWeight = glGetAttribLocation(shaderProgram, "aBoneWeight");

    uProjectionMatrix = glGetUniformLocation(shaderProgram, "uProjectionMatrix");
    uModelViewMatrix = glGetUniformLocation(shaderProgram, "uModelViewMatrix");
    uBones = glGetUniformLocation(shaderProgram, "uBones");
    uEyes = glGetUniformLocation(shaderProgram, "uEyes");
    uHat = glGetUniformLocation(shaderProgram, "uHat");
    uAoTexture = glGetUniformLocation(shaderProgram, "aoTexture");
    uNorTexture = glGetUniformLocation(shaderProgram, "norTexture");
    uHatTexture = glGetUniformLocation(shaderProgram, "hatTexture");
    uFabricTexture = glGetUniformLocation(shaderProgram, "fabricTexture");
}

void initTexture() {
    aoTexture = loadTexture("tex_ao.png");
    norTexture = loadTexture("tex_nor.png");
    hatTexture = loadTexture("tex_hat.png");
    fabricTexture = loadTexture("tex_fabric.png");
    settingsTexture = loadTexture("tex_settings.png");
}

void initBuffers() {
    glGenBuffers(1, &positionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(position), &position, GL_STATIC_DRAW);

    glGenBuffers(1, &textureCoordBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textureCoordBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uv), &uv, GL_STATIC_DRAW);

    glGenBuffers(1, &normalBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(normal), &normal, GL_STATIC_DRAW);

    glGenBuffers(1, &tangentBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, tangentBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tangent), &tangent, GL_STATIC_DRAW);

    glGenBuffers(1, &boneIndexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, boneIndexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bi), &bi, GL_STATIC_DRAW);

    glGenBuffers(1, &boneWeightBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, boneWeightBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bw), &bw, GL_STATIC_DRAW);

    glGenBuffers(1, &indexBuffer1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer1);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(t1), &t1, GL_STATIC_DRAW);

    glGenBuffers(1, &indexBuffer2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer2);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(t2), &t2, GL_STATIC_DRAW);

    glGenBuffers(1, &indexBuffer3);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer3);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(t3), &t3, GL_STATIC_DRAW);
}



GLuint loadTexture(const char* filename) {
    std::vector<unsigned char> pixels;
    unsigned width, height;
    unsigned error = lodepng::decode(pixels, width, height, filename, LCT_RGB);

    if(error != 0) {
        std::cout << "error " << error << ": " << lodepng_error_text(error) << std::endl;
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return texture;
}

GLuint loadShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);

    GLint isSuccess;
    GLchar infoLog[512];
    GLsizei size;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &isSuccess);

    if (!isSuccess) {
        glGetShaderInfoLog(shader, 512, &size, (GLchar*) &infoLog);
        printf("An error occurred compiling the shaders = %s\n", infoLog);
        glDeleteShader(shader);
        return -1;
    }

    return shader;
}

GLuint initShaderProgram(const char* vsSource, const char* fsSource,
                         GLuint* vertexShader, GLuint* fragmentShader) {
    *vertexShader = loadShader(GL_VERTEX_SHADER, vsSource);
    *fragmentShader = loadShader(GL_FRAGMENT_SHADER, fsSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, *vertexShader);
    glAttachShader(shaderProgram, *fragmentShader);
    glLinkProgram(shaderProgram);

    GLint isSuccess;
    GLchar infoLog[512];
    GLsizei size;

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isSuccess);

    if (!isSuccess) {
        glGetProgramInfoLog(shaderProgram, 512, &size, (GLchar*) &infoLog);
        printf("Unable to initialize the shader program = %s\n", infoLog);

        glDetachShader(shaderProgram, *vertexShader);
        glDetachShader(shaderProgram, *fragmentShader);

        glDeleteShader(*vertexShader);
        glDeleteShader(*fragmentShader);

        glDeleteProgram(shaderProgram);

        return 0;
    }

    return shaderProgram;
}


float headX = 0, headY = 0;

void calculateBoneMatrix() {
    if(mousePressed && !(mouseX<=250 && mouseY>=(height-50))) {
        headX = (headX + (mouseX/width*2-1))/4.0;
        headY = (headY + (mouseY/height*2-1))/4.0;
    }
    float t_ = 0.01*glm::sin(t*1.5);

    glm::mat4 mNeck1 = glm::mat4(1.0);
    glm::mat4 mNeck2 = glm::mat4(1.0);
    glm::mat4 mNeck3 = glm::mat4(1.0);
    glm::mat4 mHead = glm::mat4(1.0);
    glm::mat4 mTopLip = glm::mat4(1.0);
    glm::mat4 mBottomLip = glm::mat4(1.0);
    glm::mat4 mEyeLid_R = glm::mat4(1.0);
    glm::mat4 mEyeLid_L = glm::mat4(1.0);

    mNeck3 =
        glm::rotate(headX, glm::vec3(0.0, 0.0, 1.0)) *
        glm::rotate(headY, glm::vec3(1.0, 0.0, 0.0));

    mHead = mNeck3 *
        glm::rotate(headX, glm::vec3(0.0, 0.0, 1.0)) *
        glm::rotate(headY, glm::vec3(1.0, 0.0, 0.0));


    mTopLip = mHead * glm::translate(glm::vec3(0.0, 0.1*volume, 0.5*volume+t_));

    mBottomLip = mHead * glm::translate(glm::vec3(0.0, 0.1*volume, -0.5*volume-t_));

    float x = (fmod(t, 7) < 0.25 && displayBlink) ? glm::sin(fmod(t, 7)*4*3.14152):0;

    mEyeLid_R = mHead * glm::translate(glm::vec3(0.1*x*x, 0.0, -0.05*x*x));

    mEyeLid_L = mHead * glm::translate(glm::vec3(-0.1*x*x, 0.0, -0.05*x*x));

    float allBoneMatrix[128];

    for(int i=0; i<16; i++) allBoneMatrix[0*16+i] = mNeck1[i/4][i%4];
    for(int i=0; i<16; i++) allBoneMatrix[1*16+i] = mNeck2[i/4][i%4];
    for(int i=0; i<16; i++) allBoneMatrix[2*16+i] = mNeck3[i/4][i%4];
    for(int i=0; i<16; i++) allBoneMatrix[3*16+i] = mHead[i/4][i%4];
    for(int i=0; i<16; i++) allBoneMatrix[4*16+i] = mTopLip[i/4][i%4];
    for(int i=0; i<16; i++) allBoneMatrix[5*16+i] = mBottomLip[i/4][i%4];
    for(int i=0; i<16; i++) allBoneMatrix[6*16+i] = mEyeLid_R[i/4][i%4];
    for(int i=0; i<16; i++) allBoneMatrix[7*16+i] = mEyeLid_L[i/4][i%4];

    glUniformMatrix4fv(uBones, 8, GL_FALSE, allBoneMatrix);
}















