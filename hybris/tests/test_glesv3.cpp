/*
 * Copyright (C) 2018 TheKit <nekit1000@gmail.com>
 * Copyright (c) 2012 Carsten Munk <carsten.munk@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <mutex>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cutils/log.h>

#include "test_common.h"

const char vertex_src [] =
"#version 300 es                       \n"
"in vec4               position;       \n"
"out mediump vec2      pos;            \n"
"uniform vec4          offset;         \n"
"                                      \n"
"void main()                           \n"
"{                                     \n"
"  gl_Position = position + offset;    \n"
"  gl_Position.x += float(gl_InstanceID) * 0.5; \n"
"  gl_Position.y += float(gl_InstanceID) * 0.5; \n"
"  pos = position.xy;                  \n"
"}                                     \n";

const char fragment_src [] =
"#version 300 es                                     \n"
"in mediump vec2         pos;                        \n"
"uniform mediump float   phase;                      \n"
"out vec4                fragColor;                  \n"
"                                                    \n"
"void  main()                                        \n"
"{                                                   \n"
"  fragColor =  vec4( 1., 0.9, 0.7, 1.0 ) *          \n"
"    cos( 30.*sqrt(pos.x*pos.x + 1.5*pos.y*pos.y)    \n"
"      + atan(pos.y,pos.x) - phase );                \n"
"}                                                   \n";


GLfloat norm_x    =  0.0;
GLfloat norm_y    =  0.0;
GLfloat offset_x  = -0.5;
GLfloat offset_y  = -0.5;
GLfloat p1_pos_x  =  0.0;
GLfloat p1_pos_y  =  0.0;

GLint phase_loc;
GLint offset_loc;
GLint position_loc;

const float vertexArray[] = {
    0.0,  0.5,  0.0,
   -0.5,  0.0,  0.0,
    0.0, -0.5,  0.0,
    0.5,  0.0,  0.0,
    0.0,  0.5,  0.0
};

int main()
{
	EGLDisplay display;
	EGLConfig ecfg;
	EGLint num_config;
	EGLint attr[] = {       // some attributes to set up our egl-interface
		EGL_BUFFER_SIZE, 32,
		EGL_RENDERABLE_TYPE,
		EGL_OPENGL_ES3_BIT,
		EGL_NONE
	};
	EGLSurface surface;
	EGLint ctxattr[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	EGLContext context;

	EGLBoolean rv;

	HWComposer *win = create_hwcomposer_window();

	if (!win) {
		printf("Failed to create native window\n");
		return 1;
	}
	printf("created native window\n");
	/* Not needed with hybris window platform
	 * hybris_gralloc_initialize(0); */

	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(eglGetError() == EGL_SUCCESS);
	assert(display != EGL_NO_DISPLAY);

	rv = eglInitialize(display, 0, 0);
	assert(eglGetError() == EGL_SUCCESS);
	assert(rv == EGL_TRUE);

	eglChooseConfig((EGLDisplay) display, attr, &ecfg, 1, &num_config);
	assert(eglGetError() == EGL_SUCCESS);
	assert(rv == EGL_TRUE);

	surface = eglCreateWindowSurface((EGLDisplay) display, ecfg,
		(EGLNativeWindowType) static_cast<ANativeWindow *> (win), NULL);
	assert(eglGetError() == EGL_SUCCESS);
	assert(surface != EGL_NO_SURFACE);

	context = eglCreateContext((EGLDisplay) display, ecfg, EGL_NO_CONTEXT,
	                           ctxattr);
	assert(eglGetError() == EGL_SUCCESS);
	assert(context != EGL_NO_CONTEXT);

	assert(eglMakeCurrent((EGLDisplay) display, surface, surface,
	                      context) == EGL_TRUE);

	const char *version = (const char *)glGetString(GL_VERSION);
	assert(version);
	printf("%s\n",version);

	GLuint shaderProgram = create_program(vertex_src, fragment_src);
	glUseProgram(shaderProgram);

	position_loc = glGetAttribLocation(shaderProgram, "position");
	phase_loc = glGetUniformLocation(shaderProgram, "phase");
	offset_loc = glGetUniformLocation(shaderProgram, "offset");

	if (position_loc < 0 ||  phase_loc < 0 || offset_loc < 0) {
		return 1;
	}

	glClearColor (1., 1., 1., 1.); // background color
	float phase = 0;

	for (int i=0; i<60*60; ++i) {
		glClear(GL_COLOR_BUFFER_BIT);
		glUniform1f(phase_loc, phase);
		phase = fmodf (phase + 0.5f, 2.f * 3.141f);

		glUniform4f(offset_loc, offset_x, offset_y, 0.0, 0.0);

		glVertexAttribPointer(position_loc, 3, GL_FLOAT,
							  GL_FALSE, 0, vertexArray);
		glEnableVertexAttribArray (position_loc);
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 5, 3);

		eglSwapBuffers((EGLDisplay) display, surface);
	}

	printf("stop\n");

	return 0;
}
