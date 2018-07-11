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

// If you enconuter the error "clang.exe : error : linker command failed with exit code 1",
// Add "GLESv3;" to Properties/Linker/Input/Library dependencies

#include <cstdlib>
#include <vector>
#include <cassert>
#include <cstdio>
#include <android/native_window.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include "Police.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))

#define REP(i, m) for(int i = 0, i##Max = m; i < i##Max; i ++)
#define REV(i, m) for(int i = (m)-1; i >= 0; i --)

struct Sommet {
	float x, y, z;
	int32_t c;
};

struct SavedState {
	float angle;
	int32_t x;
	int32_t y;
};

typedef struct android_app AndroidApp;

class Engine;
static int engine_InitDisplay(Engine* e);

const char *vShader =
"uniform mat4 aMat;"
"attribute mediump vec4 aPos;"
"attribute mediump vec2 aUV;"
"attribute mediump vec4 aCol;"
"varying mediump vec2 vUV;"
"varying mediump vec4 vCol;"
"void main() {"
" gl_Position = aMat * aPos;"
" vUV = aUV;"
" vCol = aCol;"
"}";

const char *fShader =
"precision mediump float;"
"uniform sampler2D uTex;"
"varying mediump vec2 vUV;"
"varying mediump vec4 vCol;"
"void main() {"
" vec4 baseCol = texture2D(uTex, vUV.st);"
//" if(baseCol.a < 1e-8) discard;"
//" gl_FragColor = vCol;"
" gl_FragColor = vCol * baseCol;"
//" gl_FragColor = texture2D(uTex, vUV.st);"
"}";

const float mIdent[4][4] = {
{ 1,0,0,0 },
{ 0,1,0,0 },
{ 0,0,1,0 },
{ 0,0,0,1 },
};

// It does not have to be called necessarily
void VerifierSlCompilation(GLuint o, int status) {
	int compiled;
	switch (status) {
	case GL_COMPILE_STATUS:
		glGetShaderiv(o, status, &compiled); break;
	case GL_LINK_STATUS:
		glGetProgramiv(o, status, &compiled); break;
	}
	if (!compiled) {
		int len;
		glGetShaderiv(o, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> errors(len + 1);
		glGetShaderInfoLog(o, len, 0, &errors[0]);

		//MessageBoxA(NULL, &errors[0], "Error", MB_OK);
		LOGI(&errors[0]);
		assert(0); // A lieu de MessageBoxA
	}
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
		glEnableVertexAttribArray(aPos);
		glEnableVertexAttribArray(aCol);
		glEnableVertexAttribArray(aUV);

		Printf(-1, 0, 0xFF00FF00, "Ce programme execute 'printf' sur GLES2.0 sur Android avec 'Native-activity'.\n");
		Printf(-1, 0, 0xFF00FF00, "Par example, le coordonnees tapees et l'angle de la machine sont affiches.\n");
		Printf(-1, 0, 0xFFFFFFFF, "state.x: %d\nstate.y: %d\nstate.angle: %f\n", state.x, state.y, state.angle);

		eglSwapBuffers(display, surface);
		curX = 0, curY = 0;
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
	void Putchar(Sommet const* vs, int c) {
		int ty = c / 16;
		int tx = c % 16;
		float x1 = tx / 16.0, y1 = ty / 16.0, x2 = (tx + 1.0) / 16.0, y2 = (ty + 1.0) / 16.0;
		float uv[] = {
			x1,y1,
			x1,y2,
			x2,y1,
			x2,y2,
		};
		glUniform1i(uTex, 0);
		glUniformMatrix4fv(aMat, 1, false, &mIdent[0][0]);
		glBindTexture(GL_TEXTURE_2D, hTexFont);

		glVertexAttribPointer(aPos, 3, GL_FLOAT, false, sizeof(Sommet), vs);
		glVertexAttribPointer(aCol, 4, GL_UNSIGNED_BYTE, true, sizeof(Sommet), &vs[0].c);
		glVertexAttribPointer(aUV, 2, GL_FLOAT, false, 0, uv);

		glEnable(GL_ALPHA_BITS);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisable(GL_ALPHA_BITS);
	}
	void Printf(float x, float y, GLuint c, const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
#define PRINTF_MAX_LEN 256
		char buf[PRINTF_MAX_LEN];
		int len = vsnprintf(buf, sizeof(buf), fmt, ap);

		float taillePolice = 0.03;
		Sommet vs[] = {
			{ x, y, 0, 0xFFFFFFFF },	// left,top
			{ x + taillePolice, y, 0, 0xFFFFFFFF },	// right,top
			{ x, y - taillePolice, 0, 0xFFFFFFFF }, // lett,bottom
			{ x + taillePolice, y - taillePolice, 0, 0xFFFFFFFF }, // right,bottom
		};

		REP(i, len) {
			if (buf[i] == '\n') { curY++, curX = 0; continue; }
			if(x + (curX+1) * taillePolice > 1) { curY++, curX = 0; } // Enveloppe de mot
			float dx = curX * taillePolice;
			float dy = curY * taillePolice;
			vs[0].x = x + dx;
			vs[1].x = x + taillePolice + dx;
			vs[2].x = x + dx;
			vs[3].x = x + taillePolice + dx;
			vs[0].y = y - dy;
			vs[1].y = y - dy;
			vs[2].y = y - taillePolice - dy;
			vs[3].y = y - taillePolice - dy;
			Putchar(vs, buf[i]); curX++;
		}
	}
	int ChargerTexture(void const* buf, int w, int h, int) {
		GLuint hTex;
		glGenTextures(1, &hTex);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_2D, hTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
		//assert(glGetError() == GL_NO_ERROR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		return hTex;
	}
	int ChargerPoliceBitmap() {
		int tw = 8 * 16, th = 8 * 16;

		std::vector<GLuint> buf(th*tw);
		memset(&buf[0], 0, sizeof(buf[0])*buf.size());
		REP(y, th / 8) REP(x, tw / 8)
			REP(yy, 8) {
			unsigned char t = police[(tw*y + 8 * x) + yy];
			//REP(xx, 8) buf[tw * (yy + 8 * y) + xx + 8 * x] = (t & (1 << xx)) ? ~0 : 0;
			REP(xx, 8) buf[tw * (7 - xx + 8 * y) + yy + 8 * x] = (t & (1 << xx)) ? ~0 : 0; // Transpose
		}
		return hTexFont = ChargerTexture(&buf[0], tw, th, 0);
	}

	GLuint CreerProgramme(const char* pvShader, const char* pfShader) {
		GLuint hVShader = glCreateShader(GL_VERTEX_SHADER);
		GLuint hFShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(hVShader, 1, &vShader, 0);
		glShaderSource(hFShader, 1, &fShader, 0);
		glCompileShader(hVShader);
		glCompileShader(hFShader);
		VerifierSlCompilation(hVShader, GL_COMPILE_STATUS);
		VerifierSlCompilation(hFShader, GL_COMPILE_STATUS);

		hProg = glCreateProgram();
		glAttachShader(hProg, hVShader);
		glAttachShader(hProg, hFShader);
		glBindAttribLocation(hProg, aMat, "aMat");
		glBindAttribLocation(hProg, aPos, "aPos");
		glBindAttribLocation(hProg, aCol, "aCol");
		glBindAttribLocation(hProg, aUV, "aUV");
		glBindAttribLocation(hProg, uTex, "uTex");
		glLinkProgram(hProg);
		VerifierSlCompilation(hProg, GL_LINK_STATUS);
		return hProg;
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
			if (e->app->window) {
				engine_InitDisplay(e);
				//e->InitDisplay();
				e->CreerProgramme(vShader, fShader);
				e->ChargerPoliceBitmap();
				e->DrawFrame();
			}
			break;
		case APP_CMD_TERM_WINDOW:
			// The window is being hidden or closed, clean it up.
			e->TermDisplay();
			break;
		case APP_CMD_GAINED_FOCUS:
			// When our app gains focus, we start monitoring the accelerometer.
			if (e->accelerometerSensor) {
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
			if (e->accelerometerSensor) {
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
		animating = true;
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

		// Initialiser les parametres
		aPos = 0;
		aCol = 1;
		aUV = 2;
		uTex = 4;
		curX = 0;
		curY = 0;
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

	// Variables du shader
	GLuint hProg;
	int aMat, aPos, aCol, aUV, uTex;

	// Texture de police et son cursour
	GLuint hTexFont;
	int curX, curY;
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

	EGLConfig config;
	EGLint numConfigs;
	assert(eglChooseConfig(display, attribs, &config, 1, &numConfigs));

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

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glViewport(0, 0, w, h);

	return 0;
}

void android_main(AndroidApp* state) {
	for (Engine e(state); ;) {
		int ident;
		int events;
		android_poll_source* source;

		while ((ident = ALooper_pollAll(e.animating ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
			// Process this event.
			if (source) source->process(state, source);

			// If a sensor has data, process it now.
			if (ident == LOOPER_ID_USER) {
				if (e.accelerometerSensor != NULL) {
					ASensorEvent event;
					while (ASensorEventQueue_getEvents(e.sensorEventQueue, &event, 1) > 0) {
						LOGI("accelerometer: x=%f y=%f z=%f",
							event.acceleration.x, event.acceleration.y,
							event.acceleration.z);
					}
				}
			}

			// Check if we are exiting.
			if (state->destroyRequested) {
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
