//#include "nuklear.h"

void Calc(nk_context *ctx) {
	if(nk_begin(ctx, "Calc"), nk_rect(0, 64, 512, 512),
		NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE)) {
		static int prev = 0, op = 0;
		static bool set = false;
		static char *num = "789456123";
		static char *ops = "+-*/";
		static double x, y;
		static double *cur = &x;

		nk_layout_row_dynamic(ctx, 35, 1);
		char buf[256];
		int len = snprintf(buf, sizeof(buf), "%f", *cur);
		nk_edit_string(ctx, NK_EDIT_SIMPLE, buf, &len, sizeof(buf)-1, nk_filter_float);
		buf[len] = 0;
		*cur = atoi(buf);

		bool solve = false;
		nk_layout_row_dynamic(ctx, 35, 4);
		REP(i, 16) {
			if (12 <= i && i < 15) {
				if (12 < i) continue;
				if (nk_button_label(ctx, "C")) { x = y = op = set = false; cur = &x; }
				if (nk_button_label(ctx, "0")) { *cur *= 10; set = false; }
				if (nk_button_label(ctx, "=")) { solve = true; prev = op; op = 0; }
			} else if ((i + 1) & 0x03) {
				if (nk_button_text(ctx, &ops[i >> 2], 1)) {
					*cur = *cur * 10 + num[(i >> 2) * 3 + (i & 0x03)] - '0';
					set = false;
				}
			} else if (nk_button_text(ctx, &ops[i >> 2], 1))) {
				if (!set) {
					if (cur != &y) cur = &y;
					else prev = op, solve = true;
				}
				op = ops[i >> 2];
				set = true;
			}
		}
		if (solve) {
			switch (prev) {
				case '+': x += y; break;
				case '-': x -= y; break;
				case '*': x *= y; break;
				case '/': x /= y; break;
			}
			cur = &x;
			if (set) cur = &y;
			y = 0;
			set = false;
		}
	}
	nk_end(ctx);
}