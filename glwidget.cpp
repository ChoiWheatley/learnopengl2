/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "glwidget.h"
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QCoreApplication>
#include <math.h>
#include <stdlib.h>
#include <iostream>

bool GLWidget::m_transparent = false;

float vertices[] = {
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    0.0f, 0.5f, 0.0f
};

static const char *vertexShaderSourceCore =
    "#version 150\n"
    "in vec4 vertex;\n"
    "in vec3 normal;\n"
    "out vec3 vert;\n"
    "out vec3 vertNormal;\n"
    "uniform mat4 projMatrix;\n"
    "uniform mat4 mvMatrix;\n"
    "uniform mat3 normalMatrix;\n"
    "void main() {\n"
    "   vert = vertex.xyz;\n"
    "   vertNormal = normalMatrix * normal;\n"
    "   gl_Position = projMatrix * mvMatrix * vertex;\n"
    "}\n";

static const char *fragmentShaderSourceCore =
    "#version 150\n"
    "in highp vec3 vert;\n"
    "in highp vec3 vertNormal;\n"
    "out highp vec4 fragColor;\n"
    "uniform highp vec3 lightPos;\n"
    "void main() {\n"
    "   highp vec3 L = normalize(lightPos - vert);\n"
    "   highp float NL = max(dot(normalize(vertNormal), L), 0.0);\n"
    "   highp vec3 color = vec3(0.39, 1.0, 0.0);\n"
    "   highp vec3 col = clamp(color * 0.2 + color * 0.8 * NL, 0.0, 1.0);\n"
    "   fragColor = vec4(col, 1.0);\n"
    "}\n";

/// NDC (Normalized Device Coord) 로 변환하는 과정을 버텍스 셰이더에서 진행한다.
/// 로컬 스페이스에서 [-1.0, 1.0] 사이의 정규 좌표계로 이동하여야 글로벌 스페이스에서
/// 표현이 가능하다.
static const char *vertexShaderSource =
        "#version 330 core"                             // Shader begins with the declaration of its version.
        "layout (location = 0) in vec3 aPos;\n"         // Declare all the input vertex attributes in the vertex shader
                                                        //  with the `in` keyword.
                                                        // `layout (location = 0)` -> Set the location of the input variable
        "void main(){\n"
        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "}\n"
        ;

/// Fragment Shader는 픽셀에 뿌릴 정보를 계산한다. 래스터화라고도 부른다.
static const char *fragmentShaderSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"                         // We can declare output values with the `out` keyword
        "void main(){\n"
        "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"// fourth value `1.0f` means an alpha value, completely opaque
        "}\n"
        ;

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    m_core = QSurfaceFormat::defaultFormat().profile() == QSurfaceFormat::CoreProfile;
    // --transparent causes the clear color to be transparent. Therefore, on systems that
    // support it, the widget will become transparent apart from the logo.
    if (m_transparent) {
        QSurfaceFormat fmt = format();
        fmt.setAlphaBufferSize(8);
        setFormat(fmt);
    }

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_DeleteOnClose);
}

GLWidget::~GLWidget()
{
    // cleanup();
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize GLWidget::sizeHint() const
{
    return QSize(400, 400);
}


void GLWidget::initializeGL()
{
    int shader_success;
    char infoLog[512];

    /// 런타임에 버텍스 셰이더를 동적으로 컴파일 하는 과정을 보여준다.
    // provide the type of shader we want to create as an argument.
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    // Attach the shader source code to the shader object and compile the shader
    glShaderSource(m_vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(m_vertexShader);
    // Check if compilation was successful
    glGetShaderiv(m_vertexShader, GL_COMPILE_STATUS, &shader_success);
    if ( !shader_success ) {
        glGetShaderInfoLog(m_vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << '\n';
    }


    /// 런타임에 픽셀 셰이더 (=fragment shader) 를 동적으로 컴파일 하는 과정을 보여준다.
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(m_fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(m_fragmentShader);
    // Check if compilation was successful
    glGetShaderiv(m_fragmentShader, GL_COMPILE_STATUS, &shader_success);
    if ( !shader_success ) {
        glGetShaderInfoLog(m_fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << '\n';
    }

    /// '렌더링' 사전단계를 모두 마무리했다. 이제 컴파일된 셰이더를 서로 연결하여
    /// Shader Program Object 로 만들어주어야 한다.
    m_shaderProgram = glCreateProgram();
    // We need to attach the previously compiled shaders to the program object and then
    //  link them!
    glAttachShader(m_shaderProgram, m_vertexShader);
    glAttachShader(m_shaderProgram, m_fragmentShader);
    glLinkProgram(m_shaderProgram);
    // Check if compilation was successful
    glGetShaderiv(m_shaderProgram, GL_COMPILE_STATUS, &shader_success);
    if ( !shader_success ) {
        glGetShaderInfoLog(m_shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << '\n';
    }

    /// Vertex Array Object, Vertex Buffer Object를 등록하여 버텍스를 그래픽 프로세싱 유닛으로 푸시한다.
    glGenVertexArrays(1, &m_VAO);
    glBindVertexArray(m_VAO);
    glGenBuffers(1, &m_VBO);
    // Copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    /// 버텍스 데이터를 어느 버텍스 셰이더에 집어넣을지 결정해주어야 한다.
    glVertexAttribPointer(  0                   // Sets the location of the vertex attribute to 0 via
                                                //  `layout (location = 0)`
                          , 3                   // The size of the vertex attrib via `vec3 aPos`
                          , GL_FLOAT
                          , GL_FALSE            // Specifies if we want the data to be normalized.
                          , 3 * sizeof(float)   // Stride tells us the space between consecutive vertex
                                                //  attributes
                          , (void*)0);          // Offset
    // Giving the vertex attribute location as its argument
    glEnableVertexAttribArray(0);

    /// 프로그램을 사용한다.
    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_VAO);
    // Don't forget to delete the shader objects once we've linked them into the program obj.
    glDeleteShader(m_vertexShader);
    glDeleteShader(m_fragmentShader);
}

void GLWidget::setupVertexAttribs()
{
}

void GLWidget::paintGL()
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /// 프로그램을 사용한다.
    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GLWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_lastPos = event->pos();
    qDebug() << m_lastPos;
}

void GLWidget::keyPressEvent(QKeyEvent *event)
{
    qDebug() << event->key();
    if (event->key() == Qt::Key_Escape)
    {
        QWidget::close();
    } else {
    }
}

void GLWidget::keyReleaseEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

void GLWidget::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    qDebug() << "ByeBye~~\n";
}
