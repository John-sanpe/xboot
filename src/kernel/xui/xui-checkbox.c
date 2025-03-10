/*
 * kernel/xui/xui-checkbox.c
 *
 * Copyright(c) 2007-2022 Jianjun Jiang <8192542@qq.com>
 * Official site: http://xboot.org
 * Mobile phone: +86-18665388956
 * QQ: 8192542
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <xboot.h>
#include <xui/checkbox.h>

int xui_checkbox_ex(struct xui_context_t * ctx, const char * label, int * state, int opt)
{
	unsigned int id = xui_get_id(ctx, &state, sizeof(int *));
	struct region_t region, * r = xui_layout_next(ctx);
	struct xui_widget_color_t * wc;
	struct color_t * bg, * fg, * bc;
	int radius, width;
	int click = 0;

	xui_control_update(ctx, id, r, opt);
	if((ctx->active == id) && (ctx->mouse.up & XUI_MOUSE_LEFT))
	{
		*state = !*state;
		click = 1;
	}
	radius = ctx->style.checkbox.border_radius;
	width = ctx->style.checkbox.border_width;
	if(*state)
	{
		switch(opt & (0x7 << 8))
		{
		case XUI_CHECKBOX_PRIMARY:
			wc = &ctx->style.primary;
			break;
		case XUI_CHECKBOX_SECONDARY:
			wc = &ctx->style.secondary;
			break;
		case XUI_CHECKBOX_SUCCESS:
			wc = &ctx->style.success;
			break;
		case XUI_CHECKBOX_INFO:
			wc = &ctx->style.info;
			break;
		case XUI_CHECKBOX_WARNING:
			wc = &ctx->style.warning;
			break;
		case XUI_CHECKBOX_DANGER:
			wc = &ctx->style.danger;
			break;
		case XUI_CHECKBOX_LIGHT:
			wc = &ctx->style.light;
			break;
		case XUI_CHECKBOX_DARK:
			wc = &ctx->style.dark;
			break;
		default:
			wc = &ctx->style.primary;
			break;
		}
	}
	else
	{
		wc = &ctx->style.secondary;
	}
	if(ctx->active == id)
	{
		bg = &wc->active.background;
		fg = &wc->active.foreground;
		bc = &wc->active.border;
	}
	else if(ctx->hover == id)
	{
		bg = &wc->hover.background;
		fg = &wc->hover.foreground;
		bc = &wc->hover.border;
	}
	else
	{
		bg = &wc->normal.background;
		fg = &wc->normal.foreground;
		bc = &wc->normal.border;
	}
	if(bc->a && (width > 0))
		xui_draw_rectangle(ctx, r->x, r->y, r->h, r->h, radius, width, bc);
	if(bg->a)
		xui_draw_rectangle(ctx, r->x, r->y, r->h, r->h, radius, *state ? 0 : ctx->style.checkbox.outline_width, bg);
	if(*state)
		xui_draw_icon(ctx, ctx->style.font.icon_family, ctx->style.checkbox.check_icon, r->x, r->y, r->h, r->h, fg);
	if(label && fg->a)
	{
		region_init(&region, r->x + r->h, r->y, r->w - r->h, r->h);
		xui_draw_text_align(ctx, ctx->style.font.font_family, ctx->style.font.size, label, &region, 0, &ctx->style.font.color, opt);
	}
	return click;
}
