#include "draw.h"
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <algorithm>

#include "glad/glad.h"
#include "glfw/glfw3.h"
#include "imgui/imgui.h"

#define BUFFER_OFFSET(x)  ((const void*) (x))

DebugDraw g_debugDraw;
Camera g_camera;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Camera::Camera()
{
    m_center = ysVecSet(0.0f, -16.0f, 0.0f);
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_verticalFov = ys_pi * 0.25f;
    m_aspectRatio = 16.0f / 9.0f;
    m_near = 0.25f;
    m_far = 128.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Camera::BuildProjectionMatrix(ys_float32* m) const
{
    // http://www.songho.ca/opengl/gl_projectionmatrix.html
    ys_float32 h = m_near * tanf(m_verticalFov);
    ys_float32 w = h * m_aspectRatio;

    ys_float32 negInvLength = 1.0f / (m_near - m_far);

    m[0] = m_near / w;
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;

    m[4] = 0.0f;
    m[5] = m_near / h;
    m[6] = 0.0f;
    m[7] = 0.0f;

    m[8] = 0.0f;
    m[9] = 0.0f;
    m[10] = negInvLength * (m_near + m_far);
    m[11] = -1.0f;

    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = negInvLength * (2.0f * m_near * m_far);
    m[15] = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ysVec4 sComputeCameraQuat(ys_float32 yaw, ys_float32 pitch)
{
    ysVec4 qYaw = ysQuatFromAxisAngle(ysVec4_unitZ, -yaw);
    ysVec4 qPitch = ysQuatFromAxisAngle(ysVec4_unitX, ys_pi * 0.5f + pitch);
    return ysMulQQ(qYaw, qPitch);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Camera::BuildViewMatrix(ys_float32* m) const
{
    ysTransform camXf;
    camXf.p = m_center;
    camXf.q = sComputeCameraQuat(m_yaw, m_pitch);
    ysTransform viewXf = ysInvert(camXf);
    ysMtx44 viewMtx = ysMtxFromTransform(viewXf);
    ysMemCpy(m, &viewMtx, sizeof(ysMtx44));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 Camera::ComputeRight() const
{
    ysVec4 q = sComputeCameraQuat(m_yaw, m_pitch);
    return ysRotate(q, ysVec4_unitX);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 Camera::ComputeUp() const
{
    ysVec4 q = sComputeCameraQuat(m_yaw, m_pitch);
    return ysRotate(q, ysVec4_unitY);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysVec4 Camera::ComputeForward() const
{
    ysVec4 q = sComputeCameraQuat(m_yaw, m_pitch);
    return -ysRotate(q, ysVec4_unitZ);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysTransform Camera::ComputeEyeTransform() const
{
    ysTransform camXf;
    camXf.p = m_center;
    camXf.q = sComputeCameraQuat(m_yaw, m_pitch);
    return camXf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sCheckGLError()
{
    GLenum errCode = glGetError();
    if (errCode != GL_NO_ERROR)
    {
        fprintf(stderr, "OpenGL error = %d\n", errCode);
        ysAssert(false);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Prints shader compilation errors
static void sPrintLog(GLuint object)
{
    GLint log_length = 0;
    if (glIsShader(object))
    {
        glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
    }
    else if (glIsProgram(object))
    {
        glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
    }
    else
    {
        fprintf(stderr, "printlog: Not a shader or a program\n");
        return;
    }

    char* log = (char*)malloc(log_length);

    if (glIsShader(object))
    {
        glGetShaderInfoLog(object, log_length, NULL, log);
    }
    else if (glIsProgram(object))
    {
        glGetProgramInfoLog(object, log_length, NULL, log);
    }

    fprintf(stderr, "%s", log);
    free(log);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static GLuint sCreateShaderFromString(const char* source, GLenum type)
{
    GLuint res = glCreateShader(type);
    const char* sources[] = { source };
    glShaderSource(res, 1, sources, NULL);
    glCompileShader(res);
    GLint compile_ok = GL_FALSE;
    glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
    if (compile_ok == GL_FALSE)
    {
        fprintf(stderr, "Error compiling shader of type %d!\n", type);
        sPrintLog(res);
        glDeleteShader(res);
        return 0;
    }
    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static GLuint sCreateShaderProgram(const char* vs, const char* fs)
{
    GLuint vsId = sCreateShaderFromString(vs, GL_VERTEX_SHADER);
    GLuint fsId = sCreateShaderFromString(fs, GL_FRAGMENT_SHADER);
    assert(vsId != 0 && fsId != 0);

    GLuint programId = glCreateProgram();
    glAttachShader(programId, vsId);
    glAttachShader(programId, fsId);
    glBindFragDataLocation(programId, 0, "out_color");
    glLinkProgram(programId);

    glDeleteShader(vsId);
    glDeleteShader(fsId);

    GLint status = GL_FALSE;
    glGetProgramiv(programId, GL_LINK_STATUS, &status);
    assert(status != GL_FALSE);
    
    return programId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ShaderLoader
{
    // https://www.opengl.org/sdk/docs/tutorials/ClockworkCoders/loading.php

    static ys_uint32 sGetFileLength(std::ifstream& file)
    {
        if (!file.good())
        {
            return 0;
        }
        ys_uint32 pos = (ys_uint32)file.tellg();
        file.seekg(0, std::ios::end);
        ys_uint32 len = (ys_uint32)file.tellg();
        file.seekg(std::ios::beg);
        return len;
    }

    static ys_int32 sLoadShader(char** shaderSource, ys_uint32* len, char* filename)
    {
        std::ifstream file;
        file.open(filename, std::ios::in); // opens as ASCII!
        if (!file)
        {
            return -1;
        }

        *len = sGetFileLength(file);
        if (*len == 0)
        {
            return -2; // Error: Empty File 
        }

        *shaderSource = ysNew char[*len + 1];
        if (*shaderSource == nullptr)
        {
            return -3;   // can't reserve memory
        }

        // len isn't always strlen cause some characters are stripped in ascii read...
        // it is important to 0-terminate the real length later, len is just max possible value...
        (*shaderSource)[*len] = '\0';
        ys_uint32 i = 0;
        while (file.good())
        {
            (*shaderSource)[i] = file.get(); // get character from file.
            if (!file.eof())
            {
                i++;
            }
        }

        (*shaderSource)[i] = '\0'; // 0-terminate it at the correct position
        file.close();
        return 0; // No Error
    }
    
    static void sUnloadShader(char** shaderSource)
    {
        if (*shaderSource != nullptr)
        {
            ysDeleteArray(*shaderSource);
        }
        *shaderSource = nullptr;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Vector3
{
    void Set(ys_float32 xIn, ys_float32 yIn, ys_float32 zIn)
    {
        x = xIn;
        y = yIn;
        z = zIn;
    }

    ys_float32 x, y, z;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct GLRenderLines
{
    void Create()
    {
        {
            char* shaderSourceVS = nullptr;
            char* shaderSourcePS = nullptr;
            ys_uint32 lenVS = 0;
            ys_uint32 lenPS = 0;
            ys_int32 resVS = ShaderLoader::sLoadShader(&shaderSourceVS, &lenVS, "./shaders/edge_VS.glsl");
            ys_int32 resPS = ShaderLoader::sLoadShader(&shaderSourcePS, &lenPS, "./shaders/edge_PS.glsl");
            ysAssert(resVS == 0);
            ysAssert(resPS == 0);
            m_programId = sCreateShaderProgram(shaderSourceVS, shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourceVS);
        }

        m_projectionUniform = glGetUniformLocation(m_programId, "g_projectionMatrix");
        m_viewUniform = glGetUniformLocation(m_programId, "g_viewMatrix");
        m_worldUniform = glGetUniformLocation(m_programId, "cb_worldMatrix");
        ysAssert(m_projectionUniform >= 0);
        ysAssert(m_viewUniform >= 0);
        ysAssert(m_worldUniform >= 0);
        m_vertexAttribute = 0;
        m_colorAttribute = 1;

        // Generate
        glGenVertexArrays(1, &m_vaoId);
        glGenBuffers(2, m_vboIds);

        glBindVertexArray(m_vaoId);
        glEnableVertexAttribArray(m_vertexAttribute);
        glEnableVertexAttribArray(m_colorAttribute);

        // Vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
        glVertexAttribPointer(m_vertexAttribute, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
        glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[1]);
        glVertexAttribPointer(m_colorAttribute, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
        glBufferData(GL_ARRAY_BUFFER, sizeof(m_colors), m_colors, GL_DYNAMIC_DRAW);

        sCheckGLError();

        // Cleanup
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        m_count = 0;
    }
    
    void Destroy()
    {
        if (m_vaoId)
        {
            glDeleteVertexArrays(1, &m_vaoId);
            glDeleteBuffers(2, m_vboIds);
            m_vaoId = 0;
        }
        
        if (m_programId)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
        }
    }
    
    void PushVertex(const ysVec4& v, const Color& c)
    {
        if (m_count == e_maxVertices)
        {
            Flush();
        }
        m_vertices[m_count].x = v.x;
        m_vertices[m_count].y = v.y;
        m_vertices[m_count].z = v.z;
        m_colors[m_count] = c;
        ++m_count;
    }
    
    void Flush()
    {
        if (m_count == 0)
        {
            return;
        }
        
        glUseProgram(m_programId);

        ys_float32 proj[16];
        ys_float32 view[16];
        ys_float32 world[16] =
        {
            // Custom debug draw is specified in world space, so world matrix is identity
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        g_camera.BuildProjectionMatrix(proj);
        g_camera.BuildViewMatrix(view);

        glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, proj);
        glUniformMatrix4fv(m_viewUniform, 1, GL_FALSE, view);
        glUniformMatrix4fv(m_worldUniform, 1, GL_FALSE, world);
        
        glBindVertexArray(m_vaoId);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_count * sizeof(Vector3), m_vertices);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_count * sizeof(Color), m_colors);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDrawArrays(GL_LINES, 0, 2 * m_count);
        
        sCheckGLError();
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        
        m_count = 0;
    }
    
    enum { e_maxVertices = 2 * 512 };
    Vector3 m_vertices[e_maxVertices];
    Color m_colors[e_maxVertices];
    
    ys_int32 m_count;
    
    GLuint m_vaoId;
    GLuint m_vboIds[2];
    GLuint m_programId;
    GLint m_projectionUniform;
    GLint m_viewUniform;
    GLint m_worldUniform;
    GLint m_vertexAttribute;
    GLint m_colorAttribute;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct GLRenderTriangles
{
    void Create()
    {
        {
            char* shaderSourceVS = nullptr;
            char* shaderSourcePS = nullptr;
            ys_uint32 lenVS = 0;
            ys_uint32 lenPS = 0;
            ys_int32 resVS = ShaderLoader::sLoadShader(&shaderSourceVS, &lenVS, "./shaders/face_VS.glsl");
            ys_int32 resPS = ShaderLoader::sLoadShader(&shaderSourcePS, &lenPS, "./shaders/face_PS.glsl");
            ysAssert(resVS == 0);
            ysAssert(resPS == 0);
            m_programId = sCreateShaderProgram(shaderSourceVS, shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourceVS);
        }
        m_projectionUniform = glGetUniformLocation(m_programId, "g_projectionMatrix");
        m_viewUniform = glGetUniformLocation(m_programId, "g_viewMatrix");
        m_worldUniform = glGetUniformLocation(m_programId, "cb_worldMatrix");
        ysAssert(m_projectionUniform >= 0);
        ysAssert(m_viewUniform >= 0);
        ysAssert(m_worldUniform >= 0);
        m_vertexAttribute = 0;
        m_colorAttribute = 1;

        // Generate
        glGenVertexArrays(1, &m_vaoId);
        glGenBuffers(2, m_vboIds);

        glBindVertexArray(m_vaoId);
        glEnableVertexAttribArray(m_vertexAttribute);
        glEnableVertexAttribArray(m_colorAttribute);

        // Vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
        glVertexAttribPointer(m_vertexAttribute, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
        glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[1]);
        glVertexAttribPointer(m_colorAttribute, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
        glBufferData(GL_ARRAY_BUFFER, sizeof(m_colors), m_colors, GL_DYNAMIC_DRAW);

        sCheckGLError();

        // Cleanup
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        m_count = 0;
    }

    void Destroy()
    {
        if (m_vaoId)
        {
            glDeleteVertexArrays(1, &m_vaoId);
            glDeleteBuffers(2, m_vboIds);
            m_vaoId = 0;
        }

        if (m_programId)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
        }
    }

    void PushVertex(const ysVec4& v, const Color& c)
    {
        if (m_count == e_maxVertices)
        {
            Flush();
        }
        m_vertices[m_count].x = v.x;
        m_vertices[m_count].y = v.y;
        m_vertices[m_count].z = v.z;
        m_colors[m_count] = c;
        ++m_count;
    }

    void Flush()
    {
        if (m_count == 0)
        {
            return;
        }
        
        glUseProgram(m_programId);

        ys_float32 proj[16];
        ys_float32 view[16];
        ys_float32 world[16] =
        {
            // Custom debug draw is specified in world space, so world matrix is identity
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        g_camera.BuildProjectionMatrix(proj);
        g_camera.BuildViewMatrix(view);
        
        glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, proj);
        glUniformMatrix4fv(m_viewUniform, 1, GL_FALSE, view);
        glUniformMatrix4fv(m_worldUniform, 1, GL_FALSE, world);
        
        glBindVertexArray(m_vaoId);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_count * sizeof(Vector3), m_vertices);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_count * sizeof(Color), m_colors);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDrawArrays(GL_TRIANGLES, 0, 3 * m_count);
        
        sCheckGLError();
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        
        m_count = 0;
    }
    
    enum { e_maxVertices = 3 * 512 };
    Vector3 m_vertices[e_maxVertices];
    Color m_colors[e_maxVertices];

    ys_int32 m_count;

    GLuint m_vaoId;
    GLuint m_vboIds[2];
    GLuint m_programId;
    GLint m_projectionUniform;
    GLint m_viewUniform;
    GLint m_worldUniform;
    GLint m_vertexAttribute;
    GLint m_colorAttribute;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct GLRenderPrimitiveLines
{
    void Create()
    {
        {
            char* shaderSourceVS = nullptr;
            char* shaderSourcePS = nullptr;
            ys_uint32 lenVS = 0;
            ys_uint32 lenPS = 0;
            ys_int32 resVS = ShaderLoader::sLoadShader(&shaderSourceVS, &lenVS, "./shaders/primEdge_VS.glsl");
            ys_int32 resPS = ShaderLoader::sLoadShader(&shaderSourcePS, &lenPS, "./shaders/primEdge_PS.glsl");
            ysAssert(resVS == 0);
            ysAssert(resPS == 0);
            m_programId = sCreateShaderProgram(shaderSourceVS, shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourceVS);
        }

        m_uniformProjMatrix = glGetUniformLocation(m_programId, "g_projectionMatrix");
        m_uniformViewMatrix = glGetUniformLocation(m_programId, "g_viewMatrix");

        ysAssert(m_uniformProjMatrix >= 0);
        ysAssert(m_uniformViewMatrix >= 0);
        m_attributeVertex = 0;
        m_attributeColor = 1;
        m_attributeWorldMatrix = 2;

        // Generate and specify vertex format
        {
            glGenVertexArrays(PrimitiveType::e_primitiveTypeCount, m_vaoIds);
            glGenBuffers(sizeof(VBOs) / sizeof(GLuint), (GLuint*)(&m_vbos));
            for (ys_int32 i = 0; i < PrimitiveType::e_primitiveTypeCount; ++i)
            {
                glBindVertexArray(m_vaoIds[i]);
                glEnableVertexAttribArray(m_attributeVertex);
                glEnableVertexAttribArray(m_attributeColor);
                glEnableVertexAttribArray(m_attributeWorldMatrix + 0);
                glEnableVertexAttribArray(m_attributeWorldMatrix + 1);
                glEnableVertexAttribArray(m_attributeWorldMatrix + 2);
                glEnableVertexAttribArray(m_attributeWorldMatrix + 3);

                // Vertex buffer
                glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_primVertices[i]);
                glVertexAttribPointer(m_attributeVertex, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

                // Per-instance color buffer
                glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instColors);
                glVertexAttribPointer(m_attributeColor, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
                glVertexAttribDivisor(m_attributeColor, 1);

                // Per-instance world matrix buffer
                glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instWorldMatrices);
                const ys_int32 worldMtxColumnStride = 4 * sizeof(ysVec4);
                glVertexAttribPointer(m_attributeWorldMatrix + 0, 4, GL_FLOAT, GL_FALSE, worldMtxColumnStride, BUFFER_OFFSET(sizeof(ysVec4) * 0));
                glVertexAttribPointer(m_attributeWorldMatrix + 1, 4, GL_FLOAT, GL_FALSE, worldMtxColumnStride, BUFFER_OFFSET(sizeof(ysVec4) * 1));
                glVertexAttribPointer(m_attributeWorldMatrix + 2, 4, GL_FLOAT, GL_FALSE, worldMtxColumnStride, BUFFER_OFFSET(sizeof(ysVec4) * 2));
                glVertexAttribPointer(m_attributeWorldMatrix + 3, 4, GL_FLOAT, GL_FALSE, worldMtxColumnStride, BUFFER_OFFSET(sizeof(ysVec4) * 3));
                glVertexAttribDivisor(m_attributeWorldMatrix + 0, 1);
                glVertexAttribDivisor(m_attributeWorldMatrix + 1, 1);
                glVertexAttribDivisor(m_attributeWorldMatrix + 2, 1);
                glVertexAttribDivisor(m_attributeWorldMatrix + 3, 1);
            }
            glBindVertexArray(0);

            // Allocate buffers for instance data. These are shared between per-primitive-type VAOs
            glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instColors);
            glBufferData(GL_ARRAY_BUFFER, sizeof(m_instColors), m_instColors, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instWorldMatrices);
            glBufferData(GL_ARRAY_BUFFER, sizeof(m_instWorldMatrices), m_instWorldMatrices, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        // Populate vertex/index buffers per primitive type
        {
            glBindVertexArray(m_vaoIds[PrimitiveType::e_box]);
            {
                // Vertex buffer
                Vector3 vertices[8];
                vertices[0].Set(-1.0f, -1.0f, -1.0f);
                vertices[1].Set(1.0f, -1.0f, -1.0f);
                vertices[2].Set(1.0f, 1.0f, -1.0f);
                vertices[3].Set(-1.0f, 1.0f, -1.0f);
                vertices[4].Set(-1.0f, -1.0f, 1.0f);
                vertices[5].Set(1.0f, -1.0f, 1.0f);
                vertices[6].Set(1.0f, 1.0f, 1.0f);
                vertices[7].Set(-1.0f, 1.0f, 1.0f);
                glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_primVertices[PrimitiveType::e_box]);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

                // Index buffer
                ys_uint32 indices[12][2];
                indices[0][0] = 0;
                indices[0][1] = 1;
                indices[1][0] = 1;
                indices[1][1] = 2;
                indices[2][0] = 2;
                indices[2][1] = 3;
                indices[3][0] = 3;
                indices[3][1] = 0;
                indices[4][0] = 4;
                indices[4][1] = 5;
                indices[5][0] = 5;
                indices[5][1] = 6;
                indices[6][0] = 6;
                indices[6][1] = 7;
                indices[7][0] = 7;
                indices[7][1] = 4;
                indices[8][0] = 0;
                indices[8][1] = 4;
                indices[9][0] = 1;
                indices[9][1] = 5;
                indices[10][0] = 2;
                indices[10][1] = 6;
                indices[11][0] = 3;
                indices[11][1] = 7;
                primitiveIndexCount[PrimitiveType::e_box] = 24;
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vbos.m_primIndices[PrimitiveType::e_box]);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0][0], GL_STATIC_DRAW);
            }

            glBindVertexArray(m_vaoIds[PrimitiveType::e_ellipsoid]);
            {
                const ys_int32 quarterCirclePointCount = 64;
                const ys_int32 circlePointCount = 4 * quarterCirclePointCount;

                // Vertex buffer
                Vector3 vertices[3][circlePointCount];
                for (ys_int32 i = 0; i < quarterCirclePointCount; ++i)
                {
                    ys_float32 angle = (ys_pi * 0.5f) * (ys_float32(i) / quarterCirclePointCount);
                    ys_float32 c = cosf(angle);
                    ys_float32 s = sinf(angle);

                    vertices[0][quarterCirclePointCount * 0 + i].Set(c, s, 0.0f);
                    vertices[0][quarterCirclePointCount * 1 + i].Set(-s, c, 0.0f);
                    vertices[0][quarterCirclePointCount * 2 + i].Set(-c, -s, 0.0f);
                    vertices[0][quarterCirclePointCount * 3 + i].Set(s, -c, 0.0f);

                    vertices[1][quarterCirclePointCount * 0 + i].Set(0.0f, c, s);
                    vertices[1][quarterCirclePointCount * 1 + i].Set(0.0f, -s, c);
                    vertices[1][quarterCirclePointCount * 2 + i].Set(0.0f, -c, -s);
                    vertices[1][quarterCirclePointCount * 3 + i].Set(0.0f, s, -c);

                    vertices[2][quarterCirclePointCount * 0 + i].Set(s, 0.0f, c);
                    vertices[2][quarterCirclePointCount * 1 + i].Set(c, 0.0f, -s);
                    vertices[2][quarterCirclePointCount * 2 + i].Set(-s, 0.0f, -c);
                    vertices[2][quarterCirclePointCount * 3 + i].Set(-c, 0.0f, s);
                }
                glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_primVertices[PrimitiveType::e_ellipsoid]);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0][0], GL_STATIC_DRAW);

                // Index buffer
                ys_uint32 indices[3][circlePointCount][2];
                for (ys_int32 i = 0; i < circlePointCount; ++i)
                {
                    indices[0][i][0] = (i + 0) % circlePointCount;
                    indices[0][i][1] = (i + 1) % circlePointCount;

                    indices[1][i][0] = indices[0][i][0] + circlePointCount;
                    indices[1][i][1] = indices[0][i][1] + circlePointCount;

                    indices[2][i][0] = indices[1][i][0] + circlePointCount;
                    indices[2][i][1] = indices[1][i][1] + circlePointCount;
                }
                primitiveIndexCount[PrimitiveType::e_ellipsoid] = 6 * circlePointCount;
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vbos.m_primIndices[PrimitiveType::e_ellipsoid]);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0][0][0], GL_STATIC_DRAW);
            }
        }

        sCheckGLError();

        // Cleanup
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        m_primitiveCount = 0;
    }

    void Destroy()
    {
        if (m_vaoIds[0])
        {
            glDeleteVertexArrays(PrimitiveType::e_primitiveTypeCount, m_vaoIds);
            glDeleteBuffers(sizeof(VBOs) / sizeof(GLuint), (GLuint*)(&m_vbos));
            for (ys_int32 i = 0; i < PrimitiveType::e_primitiveTypeCount; ++i)
            {
                m_vaoIds[i] = 0;
                m_vbos.m_primVertices[i] = 0;
                m_vbos.m_primIndices[i] = 0;
            }
            m_vbos.m_instColors = 0;
            m_vbos.m_instWorldMatrices = 0;
        }

        if (m_programId)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
        }
    }

    void PushBox(ysVec4 halfDimensions, const ysTransform& xf, const Color& c)
    {
        if (m_primitiveCount == e_primitiveCapacity)
        {
            Flush();
        }
        PrimitiveInstance* prim = m_primitives + m_primitiveCount;
        prim->m_transform = xf;
        prim->m_scale = halfDimensions;
        prim->m_color = c;
        prim->m_type = PrimitiveType::e_box;
        prim->m_index = m_primitiveCount;
        m_primitiveCount++;
    }

    void PushEllipsoid(ysVec4 halfDimensions, const ysTransform& xf, const Color& c)
    {
        if (m_primitiveCount == e_primitiveCapacity)
        {
            Flush();
        }
        PrimitiveInstance* prim = m_primitives + m_primitiveCount;
        prim->m_transform = xf;
        prim->m_scale = halfDimensions;
        prim->m_color = c;
        prim->m_type = PrimitiveType::e_ellipsoid;
        prim->m_index = m_primitiveCount;
        m_primitiveCount++;
    }

    void Flush()
    {
        if (m_primitiveCount == 0)
        {
            return;
        }
        std::sort(m_primitives, m_primitives + m_primitiveCount, sComparePrimitiveInstances);

        glEnable(GL_DEPTH_TEST);
        glUseProgram(m_programId);
        {
            ys_float32 proj[16];
            ys_float32 view[16];
            g_camera.BuildProjectionMatrix(proj);
            g_camera.BuildViewMatrix(view);
            glUniformMatrix4fv(m_uniformProjMatrix, 1, GL_FALSE, proj);
            glUniformMatrix4fv(m_uniformViewMatrix, 1, GL_FALSE, view);

            PrimitiveType prevType = PrimitiveType::e_unknown;
            ys_int32 typeCount = 0;
            for (ys_int32 i = 0; i < m_primitiveCount; ++i)
            {
                const PrimitiveInstance* primitive = m_primitives + i;
                PrimitiveType type = primitive->m_type;
                ysAssert(type >= prevType);
                if (type != prevType)
                {
                    if (typeCount > 0)
                    {
                        glBindVertexArray(m_vaoIds[prevType]);
                        glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instColors);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, typeCount * sizeof(Color), m_instColors);
                        glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instWorldMatrices);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, typeCount * sizeof(ysMtx44), m_instWorldMatrices);
                        glDrawElementsInstanced(GL_LINES, primitiveIndexCount[prevType], GL_UNSIGNED_INT, (void*)0, typeCount);
                        sCheckGLError();
                    }
                    prevType = type;
                    typeCount = 0;
                }
                ysMtx44 worldMtx = ysMtxFromTransform(primitive->m_transform);
                worldMtx.cx = worldMtx.cx * ysSplatX(primitive->m_scale);
                worldMtx.cy = worldMtx.cy * ysSplatY(primitive->m_scale);
                worldMtx.cz = worldMtx.cz * ysSplatZ(primitive->m_scale);
                m_instWorldMatrices[typeCount] = worldMtx;
                m_instColors[typeCount] = primitive->m_color;
                typeCount++;
            }

            if (typeCount > 0)
            {
                glBindVertexArray(m_vaoIds[prevType]);
                glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instColors);
                glBufferSubData(GL_ARRAY_BUFFER, 0, typeCount * sizeof(Color), m_instColors);
                glBindBuffer(GL_ARRAY_BUFFER, m_vbos.m_instWorldMatrices);
                glBufferSubData(GL_ARRAY_BUFFER, 0, typeCount * sizeof(ysMtx44), m_instWorldMatrices);
                glDrawElementsInstanced(GL_LINES, primitiveIndexCount[prevType], GL_UNSIGNED_INT, (void*)0, typeCount);
                sCheckGLError();
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
        glUseProgram(0);

        m_primitiveCount = 0;
    }

    enum PrimitiveType
    {
        e_unknown = -1,
        e_box = 0,
        e_ellipsoid = 1,
        e_primitiveTypeCount = 2
    };

    struct PrimitiveInstance
    {
        ysTransform m_transform;
        ysVec4 m_scale;
        Color m_color;
        PrimitiveType m_type;

        // The index of the slot into which this was first pushed. It is a dummy variable to facilitate strict-weak-ordered sorting.
        ys_int32 m_index;
    };

    static bool sComparePrimitiveInstances(const PrimitiveInstance& a, const PrimitiveInstance& b)
    {
        if (a.m_type < b.m_type)
        {
            return true;
        }

        if (a.m_type > b.m_type)
        {
            return false;
        }

        ysAssert(a.m_index != b.m_index);
        return a.m_index < b.m_index;
    }

    enum { e_primitiveCapacity = 512 };
    PrimitiveInstance m_primitives[e_primitiveCapacity];
    ys_int32 m_primitiveCount;

    GLuint m_vaoIds[PrimitiveType::e_primitiveTypeCount];
    ys_int32 primitiveIndexCount[PrimitiveType::e_primitiveTypeCount];

    struct VBOs
    {
        GLuint m_primVertices[PrimitiveType::e_primitiveTypeCount];
        GLuint m_primIndices[PrimitiveType::e_primitiveTypeCount];
        GLuint m_instWorldMatrices;
        GLuint m_instColors;
    }
    m_vbos;

    ysMtx44 m_instWorldMatrices[e_primitiveCapacity];
    Color m_instColors[e_primitiveCapacity];

    GLuint m_programId;

    GLint m_uniformProjMatrix;
    GLint m_uniformViewMatrix;

    GLint m_attributeVertex;
    GLint m_attributeColor;
    GLint m_attributeWorldMatrix;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct GLRenderTexturedQuads
{
    void Create()
    {
        {
            char* shaderSourceVS = nullptr;
            char* shaderSourcePS = nullptr;
            ys_uint32 lenVS = 0;
            ys_uint32 lenPS = 0;
            ys_int32 resVS = ShaderLoader::sLoadShader(&shaderSourceVS, &lenVS, "./shaders/texturedQuad_VS.glsl");
            ys_int32 resPS = ShaderLoader::sLoadShader(&shaderSourcePS, &lenPS, "./shaders/texturedQuad_PS.glsl");
            ysAssert(resVS == 0);
            ysAssert(resPS == 0);
            m_programId = sCreateShaderProgram(shaderSourceVS, shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourcePS);
            ShaderLoader::sUnloadShader(&shaderSourceVS);
        }

        m_uniformProjMatrix = glGetUniformLocation(m_programId, "g_projectionMatrix");
        m_uniformViewMatrix = glGetUniformLocation(m_programId, "g_viewMatrix");
        m_uniformWorldMatrix = glGetUniformLocation(m_programId, "cb_worldMatrix");
        m_uniformTexture = glGetUniformLocation(m_programId, "Texture");

        ysAssert(m_uniformProjMatrix >= 0);
        ysAssert(m_uniformViewMatrix >= 0);
        ysAssert(m_uniformWorldMatrix >= 0);
        ysAssert(m_uniformTexture >= 0);
        m_attributeVertex = 0;
        m_attributeTextureUV = 1;

        // Generate and specify vertex format
        {
            glGenVertexArrays(1, &m_vaoId);
            glGenBuffers(2, m_vboIds);

            glBindVertexArray(m_vaoId);
            glEnableVertexAttribArray(m_attributeVertex);
            glEnableVertexAttribArray(m_attributeTextureUV);

            ys_float32 vertexData[6][2];

            vertexData[0][0] = -1.0f;
            vertexData[0][1] = -1.0f;

            vertexData[1][0] = 1.0f;
            vertexData[1][1] = -1.0f;

            vertexData[2][0] = 1.0f;
            vertexData[2][1] = 1.0f;

            vertexData[3][0] = -1.0f;
            vertexData[3][1] = -1.0f;

            vertexData[4][0] = 1.0f;
            vertexData[4][1] = 1.0f;

            vertexData[5][0] = -1.0f;
            vertexData[5][1] = 1.0f;

            ys_float32 uvData[6][2];

            uvData[0][0] = 0.0f;
            uvData[0][1] = 1.0f;

            uvData[1][0] = 1.0f;
            uvData[1][1] = 1.0f;

            uvData[2][0] = 1.0f;
            uvData[2][1] = 0.0f;

            uvData[3][0] = 0.0f;
            uvData[3][1] = 1.0f;

            uvData[4][0] = 1.0f;
            uvData[4][1] = 0.0f;

            uvData[5][0] = 0.0f;
            uvData[5][1] = 0.0f;

            // Vertex buffer
            glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
            glVertexAttribPointer(m_attributeVertex, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), &vertexData[0][0], GL_STATIC_DRAW);

            // Texture coordinates
            glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[1]);
            glVertexAttribPointer(m_attributeTextureUV, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
            glBufferData(GL_ARRAY_BUFFER, sizeof(uvData), &uvData[0][0], GL_STATIC_DRAW);

            sCheckGLError();
        }

        // Generate and specify vertex format
        {
            GLuint m_textureIds[e_quadCapacity];
            glGenTextures(e_quadCapacity, m_textureIds);
            for (ys_int32 i = 0; i < e_quadCapacity; ++i)
            {
                m_quads[i].m_textureId = m_textureIds[i];
                glBindTexture(GL_TEXTURE_2D, m_textureIds[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // All this is for debug draw. No need for mip-maps
            }
            sCheckGLError();
        }

        // Cleanup
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        m_quadCount = 0;
    }

    void Destroy()
    {
        if (m_vaoId)
        {
            glDeleteVertexArrays(1, &m_vaoId);
            glDeleteBuffers(2, m_vboIds);
            m_vaoId = 0;
            m_vboIds[0] = 0;
            m_vboIds[1] = 0;
        }

        for (ys_int32 i = 0; i < e_quadCapacity; ++i)
        {
            if (m_quads[i].m_textureId != 0)
            {
                glDeleteTextures(1, &m_quads[i].m_textureId);
                m_quads[i].m_textureId = 0;
            }
        }

        if (m_programId)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
        }
    }

    void PushQuad(const Texture2D& texture, ys_float32 width, ys_float32 height, const ysTransform& xf, bool useDepthTest, bool useAlphaAsCutout)
    {
        if (m_quadCount == e_quadCapacity)
        {
            Flush();
        }
        QuadInstance* quad = m_quads + m_quadCount;
        quad->m_transform = xf;
        quad->m_scaleX = width * 0.5f;
        quad->m_scaleY = height * 0.5f;
        quad->m_useDepthTest = useDepthTest;
        quad->m_useAlphaAsCutout = useAlphaAsCutout;

        glBindTexture(GL_TEXTURE_2D, quad->m_textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.m_width, texture.m_height, 0, GL_RGBA, GL_FLOAT, texture.m_pixels);

        m_quadCount++;
    }

    void Flush()
    {
        if (m_quadCount == 0)
        {
            return;
        }


        glUseProgram(m_programId);
        glBindVertexArray(m_vaoId);
        {
            const GLint textureUnit = 0;
            glUniform1i(m_uniformTexture, textureUnit);
            glActiveTexture(GL_TEXTURE0 + textureUnit);

            ys_float32 proj[16];
            ys_float32 view[16];
            g_camera.BuildProjectionMatrix(proj);
            g_camera.BuildViewMatrix(view);
            glUniformMatrix4fv(m_uniformProjMatrix, 1, GL_FALSE, proj);
            glUniformMatrix4fv(m_uniformViewMatrix, 1, GL_FALSE, view);

            glEnable(GL_DEPTH_TEST);
            {
                for (ys_int32 i = 0; i < m_quadCount; ++i)
                {
                    const QuadInstance* quad = m_quads + i;
                    if (quad->m_useDepthTest == false)
                    {
                        continue;
                    }
                    ysMtx44 worldMtx = ysMtxFromTransform(quad->m_transform);
                    worldMtx.cx = worldMtx.cx * ysSplat(quad->m_scaleX);
                    worldMtx.cy = worldMtx.cy * ysSplat(quad->m_scaleY);
                    glUniformMatrix4fv(m_uniformWorldMatrix, 1, GL_FALSE, (ys_float32*)(&worldMtx));
                    glBindTexture(GL_TEXTURE_2D, quad->m_textureId);
                    if (quad->m_useAlphaAsCutout)
                    {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }
                    else
                    {
                        glDisable(GL_BLEND);
                    }
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    sCheckGLError();
                }
            }

            glDisable(GL_DEPTH_TEST);
            {
                for (ys_int32 i = 0; i < m_quadCount; ++i)
                {
                    const QuadInstance* quad = m_quads + i;
                    if (quad->m_useDepthTest)
                    {
                        continue;
                    }
                    ysMtx44 worldMtx = ysMtxFromTransform(quad->m_transform);
                    worldMtx.cx = worldMtx.cx * ysSplat(quad->m_scaleX);
                    worldMtx.cy = worldMtx.cy * ysSplat(quad->m_scaleY);
                    glUniformMatrix4fv(m_uniformWorldMatrix, 1, GL_FALSE, (ys_float32*)(&worldMtx));
                    glBindTexture(GL_TEXTURE_2D, quad->m_textureId);
                    if (quad->m_useAlphaAsCutout)
                    {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }
                    else
                    {
                        glDisable(GL_BLEND);
                    }
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    sCheckGLError();
                }
            }
        }
        glBindVertexArray(0);
        glUseProgram(0);

        m_quadCount = 0;
    }

    struct QuadInstance
    {
        ysTransform m_transform;
        ys_float32 m_scaleX;
        ys_float32 m_scaleY;
        bool m_useDepthTest;
        bool m_useAlphaAsCutout;
        GLuint m_textureId;
    };

    enum
    {
        e_quadCapacity = 16
    };
    QuadInstance m_quads[e_quadCapacity];
    ys_int32 m_quadCount;

    GLuint m_vaoId;
    GLuint m_vboIds[2];

    GLuint m_programId;

    GLint m_uniformProjMatrix;
    GLint m_uniformViewMatrix;
    GLint m_uniformWorldMatrix;
    GLint m_uniformTexture;

    GLint m_attributeVertex;
    GLint m_attributeTextureUV;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DebugDraw::DebugDraw()
{
    m_showUI = true;
    m_drawBVH = true;
    m_drawGeo = true;
    m_drawRender = true;
    m_drawBVHDepth = -1;

    m_lines = nullptr;
    m_triangles = nullptr;
    m_primLines = nullptr;
    m_texturedQuads = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DebugDraw::~DebugDraw()
{
    ysAssert(m_lines == nullptr);
    ysAssert(m_triangles == nullptr);
    ysAssert(m_primLines == nullptr);
    ysAssert(m_texturedQuads == nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::Create()
{
    m_lines = static_cast<GLRenderLines*>(ysMalloc(sizeof(GLRenderLines)));
    m_lines->Create();
    m_triangles = static_cast<GLRenderTriangles*>(ysMalloc(sizeof(GLRenderTriangles)));
    m_triangles->Create();
    m_primLines = static_cast<GLRenderPrimitiveLines*>(ysMalloc(sizeof(GLRenderPrimitiveLines)));
    m_primLines->Create();
    m_texturedQuads = static_cast<GLRenderTexturedQuads*>(ysMalloc(sizeof(GLRenderTexturedQuads)));
    m_texturedQuads->Create();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::Destroy()
{
    m_lines->Destroy();
    ysFree(m_lines);
    m_lines = nullptr;

    m_triangles->Destroy();
    ysFree(m_triangles);
    m_triangles = nullptr;

    m_primLines->Destroy();
    ysFree(m_primLines);
    m_primLines = nullptr;

    m_texturedQuads->Destroy();
    ysFree(m_texturedQuads);
    m_texturedQuads = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::DrawSegmentList(const ysVec4* vertices, const Color* colors, ys_int32 segmentCount)
{
    for (ys_int32 i = 0; i < segmentCount; ++i)
    {
        m_lines->PushVertex(vertices[2 * i + 0], colors[i]);
        m_lines->PushVertex(vertices[2 * i + 1], colors[i]);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::DrawTriangleList(const ysVec4* vertices, const Color* colors, ys_int32 triangleCount)
{
    for (ys_int32 i = 0; i < triangleCount; ++i)
    {
        m_triangles->PushVertex(vertices[3 * i + 0], colors[i]);
        m_triangles->PushVertex(vertices[3 * i + 1], colors[i]);
        m_triangles->PushVertex(vertices[3 * i + 2], colors[i]);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::DrawWireBox(const ysVec4& halfDimensions, const ysTransform& xf, const Color& color)
{
    m_primLines->PushBox(halfDimensions, xf, color);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::DrawWireEllipsoid(const ysVec4& halfDimensions, const ysTransform& xf, const Color& color)
{
    m_primLines->PushEllipsoid(halfDimensions, xf, color);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::DrawTexturedQuad(
    const Texture2D& texture, ys_float32 width, ys_float32 height, const ysTransform& xf, bool useDepthTest, bool useAlphaAsCutout)
{
    m_texturedQuads->PushQuad(texture, width, height, xf, useDepthTest, useAlphaAsCutout);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugDraw::Flush()
{
    m_triangles->Flush();
    m_lines->Flush();
    m_primLines->Flush();
    m_texturedQuads->Flush();
}