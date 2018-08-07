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

//If you cannot compile this solution, change
//<ApplicationTypeRevision>1.0</ApplicationTypeRevision>
//to
//<ApplicationTypeRevision>2.0</ApplicationTypeRevision>
//of *.NativeActivity.vcxproj
// After that, Property->Configuration Properties->General::Platform toolset
//-> change clang setting (You can see "Clang X.X (not installed)")

// If you enconuter the error "clang.exe : error : linker command failed with exit code 1",
// Add "GLESv3;" to Properties/Linker/Input/Library dependencies

#include <linux\videodev2.h>
#include <cstdlib>
#include <vector>
#include <cassert>
//#include <OMXAL/OpenMAXAL_Android.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "NkGLES.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))

struct SavedState {
	float angle;
	int32_t x;
	int32_t y;
};

typedef struct android_app AndroidApp;

class Engine;
int initializedDisplay = false;
static int engine_init_display(Engine* eng);

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

#include <ctime>
#include <cmath>
//#include "Calc.cpp"
//#include "Calculator.c"
//#include "node_editor.c"
#include "overview.c"
//#include "style.c"

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
		LOGI("Error: %s\n", &errors[0]);
		assert(0);
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
		if (!initializedDisplay) return;
		//LOGI("[*] DrawFrame");

		if (display == NULL) return; // No display.

		const GLfloat vs[] = {
			0.0f, 0.5f, 0.0f,
			-0.5f, -0.5f, 0.0f,
			0.5f, -0.5f, 0.0f
		};

		//glClearColor(0.3f, 0.8f, 0.8f, 1.0f);
		glClearColor(0.2, 0.5, 0.8, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(hProg);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vs);
		glEnableVertexAttribArray(0);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		/* GUI */
		nk_context *ctx = &nk->ctx;
		if (nk_begin(ctx, "Demo", nk_rect(50, 50, 300, 300),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
			NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			struct nk_rect b = ctx->begin->bounds;
			struct nk_vec2 p = ctx->input.mouse.pos;

			nk_menubar_begin(ctx);
			nk_layout_row_begin(ctx, NK_STATIC, 25, 2);
			nk_layout_row_push(ctx, 45);
			if (nk_menu_begin_label(ctx, "FILE", NK_TEXT_LEFT, nk_vec2(120, 200))) {
				nk_layout_row_dynamic(ctx, 30, 1);
				nk_menu_item_label(ctx, "OPEN", NK_TEXT_LEFT);
				nk_menu_item_label(ctx, "CLOSE", NK_TEXT_LEFT);
				nk_menu_end(ctx);
			}
			nk_layout_row_push(ctx, 45);
			if (nk_menu_begin_label(ctx, "EDIT", NK_TEXT_LEFT, nk_vec2(120, 200))) {
				nk_layout_row_dynamic(ctx, 30, 1);
				nk_menu_item_label(ctx, "COPY", NK_TEXT_LEFT);
				nk_menu_item_label(ctx, "CUT", NK_TEXT_LEFT);
				nk_menu_item_label(ctx, "PASTE", NK_TEXT_LEFT);
				nk_menu_end(ctx);
			}
			nk_layout_row_end(ctx);
			nk_menubar_end(ctx);

			enum { EASY, HARD };
			static int op = EASY;
			static int property = 20;
			nk_layout_row_static(ctx, 30, 80, 1);
			if (nk_button_label(ctx, "button"))
				fprintf(stdout, "button pressed\n");
			char buf[128];
			nk_button_label(ctx, nk_itoa(buf, state.x));
			nk_button_label(ctx, nk_itoa(buf, state.y));
			nk_button_label(ctx, nk_itoa(buf, b.x));
			nk_button_label(ctx, nk_itoa(buf, b.y));
			nk_button_label(ctx, nk_itoa(buf, p.x));
			nk_button_label(ctx, nk_itoa(buf, p.y));
			nk_layout_row_dynamic(ctx, 40, 2);
			if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
			if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
			nk_layout_row_dynamic(ctx, 25, 1);
			nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);
		}
		nk_end(ctx);

		//Calc(ctx);
		//calculator(ctx);
		//node_editor(ctx);
		overview(ctx);

		nk->Render(NK_ANTI_ALIASING_ON);
		
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

	static int32_t HandleInput(AndroidApp* app, AInputEvent* e) {
		Engine* eng = (Engine*)app->userData;
		switch (AInputEvent_getType(e)) {
			case AINPUT_EVENT_TYPE_MOTION: { // Touch event
				eng->state.x = AMotionEvent_getX(e, 0);
				eng->state.y = AMotionEvent_getY(e, 0);
				float x = AMotionEvent_getX(e, 0);
				float y = AMotionEvent_getY(e, 0);

				nk_context *ctx = &eng->nk->ctx;
				nk_input_begin(ctx);
				switch (AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_MASK) {
					case AMOTION_EVENT_ACTION_DOWN: // WM_LBUTTONDOWN
						ctx->input.mouse.pos = nk_vec2(x, y);
						nk_input_button(ctx, NK_BUTTON_LEFT, x, y, true);
						break;
					case AMOTION_EVENT_ACTION_UP: // WM_LBUTTONUP
						ctx->input.mouse.pos = nk_vec2(0, 0);
						nk_input_button(ctx, NK_BUTTON_LEFT, x, y, false);
						break;
					case AMOTION_EVENT_ACTION_MOVE: // WM_MOUSEMOVE
						nk_input_motion(ctx, x, y);
						break;
				}
				nk_input_end(ctx);
			} return 1;
			case AINPUT_EVENT_TYPE_KEY:
				break;
		}
		return 0;
	}

	static void HandleCmd(AndroidApp* app, int32_t cmd) {
		Engine* eng = (Engine*)app->userData;

		switch (cmd) {
			case APP_CMD_SAVE_STATE: {
				// The system has asked us to save our current state.  Do so.
				eng->app->savedState = malloc(sizeof(SavedState));
				*((SavedState*)eng->app->savedState) = eng->state;
				eng->app->savedStateSize = sizeof(SavedState);
				break;
			}
			case APP_CMD_INIT_WINDOW:
				// The window is being shown, get it ready.
				if (eng->app->window != NULL) {
					engine_init_display(eng);
					eng->DrawFrame();
				}
				break;
			case APP_CMD_TERM_WINDOW:
				// The window is being hidden or closed, clean it up.
				eng->TermDisplay();
				break;
			case APP_CMD_GAINED_FOCUS:
				// When our app gains focus, we start monitoring the accelerometer.
				if (eng->accelerometerSensor != NULL) {
					ASensorEventQueue_enableSensor(eng->sensorEventQueue,
						eng->accelerometerSensor);
					// We'd like to get 60 events per second (in us).
					ASensorEventQueue_setEventRate(eng->sensorEventQueue,
						eng->accelerometerSensor, (1000L / 60) * 1000);
				} break;
			case APP_CMD_LOST_FOCUS:
				// When our app loses focus, we stop monitoring the accelerometer.
				// This is to avoid consuming battery while not being used.
				if (eng->accelerometerSensor) 
					ASensorEventQueue_disableSensor(eng->sensorEventQueue, eng->accelerometerSensor);

				// Also stop animating.
				eng->animating = 0;
				eng->DrawFrame();
				break;
		}
	}

	Engine(AndroidApp *state_) : animating(true), motion(0){
		// Process events that glue library pushed
		memset(&state, 0, sizeof(state));
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

		// We are starting with a previous saved state; restore from it.
		if (state_->savedState != NULL) state = *(SavedState*)state_->savedState;
	}
	~Engine() {
		delete nk;
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
	int motion;

	NkGLES *nk;
};

static int engine_init_display(Engine* eng) {
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
	ANativeWindow_setBuffersGeometry(eng->app->window, 0, 0, format);

	EGLSurface surface = eglCreateWindowSurface(display, config, eng->app->window, NULL);

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

	eng->display = display;
	eng->context = context;
	eng->surface = surface;
	eng->width = w;
	eng->height = h;
	eng->state.angle = 0;

	// Initialize GL state.
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	//glEnable(GL_CULL_FACE);
	//glShadeModel(GL_SMOOTH);
	//glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, w, h);

	eng->hProg = CreateProgram(vShader, fShader);

	initializedDisplay = true;
	eng->nk = new NkGLES(eng->display, eng->surface, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
	return 0;
}

void android_main(AndroidApp* state) {
	Engine eng(state);

	while (1) {
		int ident;
		int events;
		android_poll_source* source;

		while ((ident = ALooper_pollAll(eng.animating ? 0 : -1, NULL, &events,
			(void**)&source)) >= 0) {

			if (source) source->process(state, source);
			if (ident == LOOPER_ID_USER) {
				if (eng.accelerometerSensor != NULL) {
					ASensorEvent event;
					while (ASensorEventQueue_getEvents(eng.sensorEventQueue,
						&event, 1) > 0) {
						LOGI("accelerometer: x=%f y=%f z=%f",
							event.acceleration.x, event.acceleration.y,
							event.acceleration.z);
					}
				}
			}

			if (state->destroyRequested != 0) { eng.TermDisplay(); return; }
		}

		if (eng.animating) {
			eng.state.angle += .01f;
			if (eng.state.angle > 1) eng.state.angle = 0;
			eng.DrawFrame();
		}
	}
}
