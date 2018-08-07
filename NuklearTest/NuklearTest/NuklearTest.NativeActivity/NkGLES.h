#pragma once

#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"
#include <vector>
#include <string>

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#define REP(i, m) for(int i = 0; i < m; i ++)
extern void CheckCompiled(GLuint o, int status);
class NkGLES {
	typedef struct {
		float pos[2];
		float uv[2];
		unsigned char col[4];
	} Vertex;

	struct Device {
		struct nk_buffer cmds;
		struct nk_draw_null_texture null;
		GLuint vbo, ebo;
		GLuint hProg;

		GLuint vertShdr, fragShdr;
		GLint aPos, aUV, aCol, uTex, uMtx; // Vars used in shaders

		GLuint fontTex;
		GLsizei vs;
		size_t vp, vt, vc;

		Device() {
			GLint status;
			static const char *vShader =
				"uniform mat4 uMtx;\n"
				"attribute vec2 aPos;\n"
				"attribute vec2 aUV;\n"
				"attribute vec4 aCol;\n"
				"varying vec2 vUV;\n"
				"varying vec4 vCol;\n"
				"void main() {\n"
				"   vUV = aUV;\n"
				"   vCol = aCol;\n"
				"   gl_Position = uMtx * vec4(aPos.xy, 0, 1);\n"
				"}\n";
			static const char *fShader =
				"precision mediump float;\n"
				"uniform sampler2D uTex;\n"
				"varying vec2 vUV;\n"
				"varying vec4 vCol;\n"
				"void main() {\n"
				"   gl_FragColor = vCol * texture2D(uTex, vUV);\n"
				"}\n";

			nk_buffer_init_default(&cmds);
			hProg = glCreateProgram();
			vertShdr = glCreateShader(GL_VERTEX_SHADER);
			fragShdr = glCreateShader(GL_FRAGMENT_SHADER);

			glShaderSource(vertShdr, 1, &vShader, 0);
			glShaderSource(fragShdr, 1, &fShader, 0);
			glCompileShader(vertShdr);
			glCompileShader(fragShdr);
			glGetShaderiv(vertShdr, GL_COMPILE_STATUS, &status);
			assert(status == GL_TRUE);
			glGetShaderiv(fragShdr, GL_COMPILE_STATUS, &status);
			assert(status == GL_TRUE);
			//CheckCompiled(vertShdr, GL_COMPILE_STATUS);
			//CheckCompiled(fragShdr, GL_COMPILE_STATUS);

			glAttachShader(hProg, vertShdr);
			glAttachShader(hProg, fragShdr);
			glLinkProgram(hProg);
			glGetProgramiv(hProg, GL_LINK_STATUS, &status);
			assert(status == GL_TRUE);
			//CheckCompiled(hProg, GL_LINK_STATUS);

			uTex = glGetUniformLocation(hProg, "uTex");
			uMtx = glGetUniformLocation(hProg, "uMtx");
			aPos = glGetAttribLocation(hProg, "aPos");
			aUV = glGetAttribLocation(hProg, "aUV");
			aCol = glGetAttribLocation(hProg, "aCol");
			{
				vs = sizeof(Vertex);
				vp = offsetof(Vertex, pos);
				vt = offsetof(Vertex, uv);
				vc = offsetof(Vertex, col);
				glGenBuffers(1, &vbo);
				glGenBuffers(1, &ebo);
			}
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
		~Device() {
			glDetachShader(hProg, vertShdr);
			glDetachShader(hProg, fragShdr);
			glDeleteShader(vertShdr);
			glDeleteShader(fragShdr);
			glDeleteProgram(hProg);
			glDeleteTextures(1, &fontTex);
			glDeleteBuffers(1, &vbo);
			glDeleteBuffers(1, &ebo);
			nk_buffer_free(&cmds);
		}

		void UploadAtlas(const void *image, int width, int height) {
			glGenTextures(1, &fontTex);
			glBindTexture(GL_TEXTURE_2D, fontTex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, image);
		}
	};

	Device d;
	nk_font_atlas atlas;
	EGLDisplay disp;
	EGLSurface surf;
	std::vector<GLubyte> vertices, elements;
	std::string cbText;

public:
	nk_context ctx;

	void Render(enum nk_anti_aliasing AA) {
		int w, h;
		eglQuerySurface(disp, surf, EGL_WIDTH, &w);
		eglQuerySurface(disp, surf, EGL_HEIGHT, &h);

		float width = w, height = h, display_width = w, display_height = h;

		float ortho[4][4] = {
			{ 2/width, 0, 0, 0 },
			{ 0,-2/ height, 0, 0 },
			{ 0, 0,-1, 0 },
			{ -1,1, 0, 1 },
		};
		struct nk_vec2 scale = { display_width / width ,display_height / height };
		// setup global state
		glViewport(0, 0, display_width, display_height);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);
		glActiveTexture(GL_TEXTURE0);

		// setup program
		glUseProgram(d.hProg);
		glUniform1i(d.uTex, 0);
		glUniformMatrix4fv(d.uMtx, 1, GL_FALSE, &ortho[0][0]);
		{
			/* convert from command queue into draw list and draw to screen */
			const struct nk_draw_command *cmd;
			const nk_draw_index *offset = NULL;

			/* Bind buffers */
			glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ebo);
			{ // buffer setup
				glEnableVertexAttribArray((GLuint)d.aPos);
				glEnableVertexAttribArray((GLuint)d.aUV);
				glEnableVertexAttribArray((GLuint)d.aCol);

				glVertexAttribPointer((GLuint)d.aPos, 2, GL_FLOAT, GL_FALSE, d.vs, (void*)d.vp);
				glVertexAttribPointer((GLuint)d.aUV, 2, GL_FLOAT, GL_FALSE, d.vs, (void*)d.vt);
				glVertexAttribPointer((GLuint)d.aCol, 4, GL_UNSIGNED_BYTE, GL_TRUE, d.vs, (void*)d.vc);
			}

			glBufferData(GL_ARRAY_BUFFER, vertices.size(), NULL, GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size(), NULL, GL_STREAM_DRAW);

			// load vertices/elements directly into vertex/element buffer
			{
				// fill convert configuration
				struct nk_convert_config config;
				static const struct nk_draw_vertex_layout_element vertex_layout[] = {
					{ NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(Vertex, pos) },
					{ NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(Vertex, uv) },
					{ NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(Vertex, col) },
					{ NK_VERTEX_LAYOUT_END }
				};
				NK_MEMSET(&config, 0, sizeof(config));
				config.vertex_layout = vertex_layout;
				config.vertex_size = sizeof(Vertex);
				config.vertex_alignment = NK_ALIGNOF(Vertex);
				config.null = d.null;
				config.circle_segment_count = 22;
				config.curve_segment_count = 22;
				config.arc_segment_count = 22;
				config.global_alpha = 1.0f;
				config.shape_AA = AA;
				config.line_AA = AA;

				// setup buffers to load vertices and elements
				{struct nk_buffer vbuf, ebuf;
				nk_buffer_init_fixed(&vbuf, &vertices[0], vertices.size());
				nk_buffer_init_fixed(&ebuf, &elements[0], elements.size());
				nk_convert(&ctx, &d.cmds, &vbuf, &ebuf, &config); }
			}
			glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size(), &vertices[0]);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, elements.size(), &elements[0]);

			// iterate over and execute each draw command
			nk_draw_foreach(cmd, &ctx, &d.cmds) {
				if (!cmd->elem_count) continue;
				glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
				glScissor((GLint)(cmd->clip_rect.x * scale.x),
					(GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
					(GLint)(cmd->clip_rect.w * scale.x),
					(GLint)(cmd->clip_rect.h * scale.y));
				glDrawElements(GL_TRIANGLES, cmd->elem_count, GL_UNSIGNED_SHORT, offset);
				offset += cmd->elem_count;
			}
			nk_clear(&ctx);
		}

		glUseProgram(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
	}

	int Touch(int x, int y) {
		nk_input_begin(&ctx);
		nk_input_motion(&ctx, x, y);
		//nk_input_button(&ctx, NK_BUTTON_LEFT, x, y, 1);
		nk_input_end(&ctx);
	}

	/*
	int HandleEvent(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
		switch (msg) {
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP: {
			int down = !((lp >> 31) & 1);
			int ctrl = GetKeyState(VK_CONTROL) & (1 << 15);

		

			switch (wp) {
			case VK_SHIFT:
			case VK_LSHIFT:
			case VK_RSHIFT: nk_input_key(&ctx, NK_KEY_SHIFT, down); return 1;
			case VK_DELETE: nk_input_key(&ctx, NK_KEY_DEL, down); return 1;
			case VK_RETURN: nk_input_key(&ctx, NK_KEY_ENTER, down); return 1;
			case VK_TAB: nk_input_key(&ctx, NK_KEY_TAB, down); return 1;

			case VK_LEFT:
				if (ctrl) nk_input_key(&ctx, NK_KEY_TEXT_WORD_LEFT, down);
				else nk_input_key(&ctx, NK_KEY_LEFT, down);
				return 1;

			case VK_RIGHT:
				if (ctrl) nk_input_key(&ctx, NK_KEY_TEXT_WORD_RIGHT, down);
				else nk_input_key(&ctx, NK_KEY_RIGHT, down);
				return 1;

			case VK_BACK: nk_input_key(&ctx, NK_KEY_BACKSPACE, down); return 1;

			case VK_HOME:
				nk_input_key(&ctx, NK_KEY_TEXT_START, down);
				nk_input_key(&ctx, NK_KEY_SCROLL_START, down);
				return 1;

			case VK_END:
				nk_input_key(&ctx, NK_KEY_TEXT_END, down);
				nk_input_key(&ctx, NK_KEY_SCROLL_END, down);
				return 1;

			case VK_NEXT: nk_input_key(&ctx, NK_KEY_SCROLL_DOWN, down); return 1;
			case VK_PRIOR: nk_input_key(&ctx, NK_KEY_SCROLL_UP, down); return 1;

			case 'C': if (ctrl) {
					nk_input_key(&ctx, NK_KEY_COPY, down);
					return 1;
				} break;
			case 'V': if (ctrl) {
					nk_input_key(&ctx, NK_KEY_PASTE, down);
					return 1;
				} break;
			case 'X': if (ctrl) {
					nk_input_key(&ctx, NK_KEY_CUT, down);
					return 1;
				} break;
			case 'Z': if (ctrl) {
					nk_input_key(&ctx, NK_KEY_TEXT_UNDO, down);
					return 1;
				} break;
			case 'R': if (ctrl) {
					nk_input_key(&ctx, NK_KEY_TEXT_REDO, down);
					return 1;
				} break;
			}
			return 0;
		}

		case WM_CHAR:
			if (wp >= 32) {
				nk_input_unicode(&ctx, (nk_rune)wp);
				return 1;
			}
			break;

		case WM_LBUTTONDOWN:
			nk_input_button(&ctx, NK_BUTTON_LEFT, (short)LOWORD(lp), (short)HIWORD(lp), 1);
			SetCapture(hWnd);
			return 1;

		case WM_LBUTTONUP:
			nk_input_button(&ctx, NK_BUTTON_DOUBLE, (short)LOWORD(lp), (short)HIWORD(lp), 0);
			nk_input_button(&ctx, NK_BUTTON_LEFT, (short)LOWORD(lp), (short)HIWORD(lp), 0);
			ReleaseCapture();
			return 1;

		case WM_RBUTTONDOWN:
			nk_input_button(&ctx, NK_BUTTON_RIGHT, (short)LOWORD(lp), (short)HIWORD(lp), 1);
			SetCapture(hWnd);
			return 1;

		case WM_RBUTTONUP:
			nk_input_button(&ctx, NK_BUTTON_RIGHT, (short)LOWORD(lp), (short)HIWORD(lp), 0);
			ReleaseCapture();
			return 1;

		case WM_MBUTTONDOWN:
			nk_input_button(&ctx, NK_BUTTON_MIDDLE, (short)LOWORD(lp), (short)HIWORD(lp), 1);
			SetCapture(hWnd);
			return 1;

		case WM_MBUTTONUP:
			nk_input_button(&ctx, NK_BUTTON_MIDDLE, (short)LOWORD(lp), (short)HIWORD(lp), 0);
			ReleaseCapture();
			return 1;

		case WM_MOUSEWHEEL:
			nk_input_scroll(&ctx, nk_vec2(0, (float)(short)HIWORD(wp) / WHEEL_DELTA));
			return 1;

		case WM_MOUSEMOVE:
			nk_input_motion(&ctx, (short)LOWORD(lp), (short)HIWORD(lp));
			return 1;

		case WM_LBUTTONDBLCLK:
			nk_input_button(&ctx, NK_BUTTON_DOUBLE, (short)LOWORD(lp), (short)HIWORD(lp), 1);
			return 1;
		}

		return 0;
	}
	*/
	static void ClipboardPaste(nk_handle usr, struct nk_text_edit *edit) {
		NkGLES *thiz = (NkGLES *)usr.ptr;
		if (thiz->cbText.size()) nk_textedit_paste(edit, thiz->cbText.c_str(), thiz->cbText.size());
	}
	static void ClipboardCopy(nk_handle usr, const char *text, int len) {
		NkGLES *thiz = (NkGLES *)usr.ptr;
		if (!len) return;
		//thiz->cbText.resize(len);
		thiz->cbText = text;
	}

	nk_context *Context() { return &ctx; }

	void SetEGLContext(EGLDisplay disp_, EGLSurface surf_) { disp = disp_, surf = surf_; }

	NkGLES(EGLDisplay disp_, EGLSurface surf_, size_t maxVertBuf, size_t maxElemBuf) :
	disp(disp_), surf(surf_), d() {
		vertices.resize(maxVertBuf);
		elements.resize(maxElemBuf);

		nk_font_atlas_init_default(&atlas);
		nk_font_atlas_begin(&atlas);

		int w, h;
		nk_font *font = nk_font_atlas_add_default(&atlas, 24, NULL);
		const void *image = nk_font_atlas_bake(&atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
		d.UploadAtlas(image, w, h);
		nk_font_atlas_end(&atlas, nk_handle_id((int)d.fontTex), &d.null);
		if (atlas.default_font) nk_style_set_font(&ctx, &atlas.default_font->handle);

		nk_init_default(&ctx, &font->handle);
		ctx.clip.copy = ClipboardCopy;
		ctx.clip.paste = ClipboardPaste;
		ctx.clip.userdata = nk_handle_ptr(this);
	}

	~NkGLES() {
		nk_font_atlas_clear(&atlas);
		nk_free(&ctx);
	}
};
