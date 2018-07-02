/*
* Copyright (C) 2010 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/


//If you not compile this solution, change
//<ApplicationTypeRevision>1.0</ApplicationTypeRevision>
//to
//<ApplicationTypeRevision>2.0</ApplicationTypeRevision>
//of *.NativeActivity.vcxproj

// If you enconuter the error "clang.exe : error : linker command failed with exit code 1",
// Add "GLESv3;" to Properties/Linker/Input/Library dependencies

#include <linux\videodev2.h>
#include <cstdlib>
#include <vector>
#include <cassert>
#include <android/native_window.h>
#include <GLES/gl.h>
#include <GLES3/gl3.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))

struct SavedState {
	float angle;
	int32_t x;
	int32_t y;
};

typedef struct android_app AndroidApp;

class Engine;
static int engine_InitDisplay(Engine* e);

const char *vShader =
"attribute vec4 vPosition;"
"void main() {"
" gl_Position = vPosition;"
"}";

const char *fShader =
"precision mediump float;"
"void main() {"
" gl_FragColor = vec4(1,0,1,1);"
"}";

// It does not have to be called necessarily
void CheckCompiled(GLuint o, int status) {
	int compiled;
	switch (status) {
	case GL_COMPILE_STATUS:
		glGetShaderiv(o, status, &compiled); break;
	case GL_LINK_STATUS:
		glGetProgramiv(o, status, &compiled); break;
	}
	assert(compiled);
	if (!compiled) {
		int len;
		glGetShaderiv(o, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> errors(len + 1);
		glGetShaderInfoLog(o, len, 0, &errors[0]);

		//MessageBoxA(NULL, &errors[0], "Error", MB_OK);
		assert(0); // Instead of MessageBoxA
	}
}

GLuint CreateProgram(const char* pvShader, const char* pfShader) {
	GLuint hVShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint hFShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(hVShader, 1, &pvShader, 0);
	glShaderSource(hFShader, 1, &pfShader, 0);
	glCompileShader(hVShader);
	glCompileShader(hFShader);
	CheckCompiled(hVShader, GL_COMPILE_STATUS);
	CheckCompiled(hFShader, GL_COMPILE_STATUS);

	GLuint hProg = glCreateProgram();
	glAttachShader(hProg, hVShader);
	glAttachShader(hProg, hFShader);
	glBindAttribLocation(hProg, 0, "vPosition");
	glLinkProgram(hProg);
	CheckCompiled(hProg, GL_LINK_STATUS);

	return hProg;
}

class Engine {
public:
	void DrawFrame() {
		LOGI("[*] DrawFrame");

		if (display == NULL) return; // No display.

		const GLfloat vs[] = {
			0.0f, 0.5f, 0.0f,
			-0.5f, -0.5f, 0.0f,
			0.5f, -0.5f, 0.0f
		};

		glClearColor(0.2, 0.5, 0.8, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(hProg);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vs);
		glEnableVertexAttribArray(0);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		eglSwapBuffers(display, surface);
	}
	void TermDisplay() {
		if (display != EGL_NO_DISPLAY) {
			eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
			if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
			eglTerminate(display);
		}
		animating = 0;
		display = EGL_NO_DISPLAY;
		context = EGL_NO_CONTEXT;
		surface = EGL_NO_SURFACE;
	}

	//Process the next input event.
	static int32_t HandleInput(AndroidApp* app, AInputEvent* event) {
		Engine* e = (Engine*)app->userData;
		if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
			e->state.x = AMotionEvent_getX(event, 0);
			e->state.y = AMotionEvent_getY(event, 0);
			return 1;
		}
		return 0;
	}

	// Process the next main command.
	static void HandleCmd(AndroidApp* app, int32_t cmd) {
		Engine* e = (Engine*)app->userData;
		switch (cmd) {
		case APP_CMD_SAVE_STATE: {
			// The system has asked us to save our current state.  Do so.
			e->app->savedState = malloc(sizeof(SavedState));
			*((SavedState*)e->app->savedState) = e->state;
			e->app->savedStateSize = sizeof(SavedState);
			break;
		}
		case APP_CMD_INIT_WINDOW:
			// The window is being shown, get it ready.
			if (e->app->window != NULL) {
				engine_InitDisplay(e);
				e->DrawFrame();
			}
			break;
		case APP_CMD_TERM_WINDOW:
			// The window is being hidden or closed, clean it up.
			e->TermDisplay();
			break;
		case APP_CMD_GAINED_FOCUS:
			// When our app gains focus, we start monitoring the accelerometer.
			if (e->accelerometerSensor != NULL) {
				ASensorEventQueue_enableSensor(e->sensorEventQueue,
					e->accelerometerSensor);
				// We'd like to get 60 events per second (in us).
				ASensorEventQueue_setEventRate(e->sensorEventQueue,
					e->accelerometerSensor, (1000L / 60) * 1000);
			}
			break;
		case APP_CMD_LOST_FOCUS:
			// When our app loses focus, we stop monitoring the accelerometer.
			// This is to avoid consuming battery while not being used.
			if (e->accelerometerSensor != NULL) {
				ASensorEventQueue_disableSensor(e->sensorEventQueue,
					e->accelerometerSensor);
			}
			// Also stop animating.
			e->animating = 0;
			e->DrawFrame();
			break;
		}
	}

	Engine(AndroidApp *state_) {
		// Process events that glue library pushed
		memset(this, 0, sizeof(*this));
		state_->userData = this;
		state_->onAppCmd = Engine::HandleCmd;
		state_->onInputEvent = Engine::HandleInput; // Even handler
		app = state_;

		// Prepare to monitor accelerometer
		sensorManager = ASensorManager_getInstance();
		accelerometerSensor = ASensorManager_getDefaultSensor(sensorManager,
			ASENSOR_TYPE_ACCELEROMETER);
		sensorEventQueue = ASensorManager_createEventQueue(sensorManager,
			state_->looper, LOOPER_ID_USER, NULL, NULL);

		if (state_->savedState != NULL) // We are starting with a previous saved state; restore from it.
			state = *(SavedState*)state_->savedState;
	}

	AndroidApp* app;

	ASensorManager* sensorManager;
	const ASensor* accelerometerSensor;
	ASensorEventQueue* sensorEventQueue;

	int animating;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	int32_t width;
	int32_t height;
	SavedState state;
	GLuint hProg;
};

static int engine_InitDisplay(Engine* e) {
	const EGLint attribs[] = {
//		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};

	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	eglInitialize(display, 0, 0);
	eglBindAPI(EGL_OPENGL_ES_API);

	/* Here, the application chooses the configuration it desires. In this
	* sample, we have a very simplified selection process, where we pick
	* the first EGLConfig that matches our criteria */
	EGLConfig config;
	EGLint numConfigs;
	assert(eglChooseConfig(display, attribs, &config, 1, &numConfigs));

	/* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
	* guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
	* As soon as we picked a EGLConfig, we can safely reconfigure the
	* ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
	EGLint format;
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow_setBuffersGeometry(e->app->window, 0, 0, format);

	EGLSurface surface = eglCreateWindowSurface(display, config, e->app->window, NULL);

	const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE,
	};
	EGLContext context = eglCreateContext(display, config, NULL, contextAttribs);

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
		LOGW("Unable to eglMakeCurrent");
		return -1;
	}

	EGLint w, h;
	eglQuerySurface(display, surface, EGL_WIDTH, &w);
	eglQuerySurface(display, surface, EGL_HEIGHT, &h);

	e->display = display;
	e->context = context;
	e->surface = surface;
	e->width = w;
	e->height = h;
	e->state.angle = 0;

	// Initialize GL state.
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	//glEnable(GL_CULL_FACE);
	//glShadeModel(GL_SMOOTH);
	//glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, w, h);

	e->hProg = CreateProgram(vShader, fShader);

	return 0;
}

void android_main(AndroidApp* state) {
	Engine e(state);

	e.animating = 1;

	while (1) { // loop waiting for stuff to do.
				// Read all pending events.
		int ident;
		int events;
		struct android_poll_source* source;

		while ((ident = ALooper_pollAll(e.animating ? 0 : -1, NULL, &events,
			(void**)&source)) >= 0) {

			// Process this event.
			if (source != NULL) source->process(state, source);

			// If a sensor has data, process it now.
			if (ident == LOOPER_ID_USER) {
				if (e.accelerometerSensor != NULL) {
					ASensorEvent event;
					while (ASensorEventQueue_getEvents(e.sensorEventQueue,
						&event, 1) > 0) {
						LOGI("accelerometer: x=%f y=%f z=%f",
							event.acceleration.x, event.acceleration.y,
							event.acceleration.z);
					}
				}
			}

			// Check if we are exiting.
			if (state->destroyRequested != 0) {
				e.TermDisplay();
				return;
			}
		}

		if (e.animating) {
			e.state.angle += .01f;
			if (e.state.angle > 1) e.state.angle = 0;
			e.DrawFrame();
		}
	}
}
