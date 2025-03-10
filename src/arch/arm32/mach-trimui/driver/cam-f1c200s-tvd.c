/*
 * driver/cam-f1c200s-tvd.c
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
#include <clk/clk.h>
#include <reset/reset.h>
#include <interrupt/interrupt.h>
#include <camera/camera.h>

enum {
	TVD_ENHANCE1	= 0x000c,
	TVD_ENHANCE2	= 0x0010,
	TVD_YADDR		= 0x0080,
	TVD_CADDR		= 0x0084,
	TVD_IRQ_STATUS	= 0x0094,
	TVD_IRQ_CTL		= 0x009c,
	TVD_MODE		= 0x0f14,
	TVD_CHROM		= 0x0f1c,
};

enum tvd_source_t {
	TVD_SOURCE_NTSC,
	TVD_SOURCE_PAL,
};

enum tvd_foramt_t {
	TVD_PL_YUV422,
	TVD_PL_YUV420,
	TVD_MB_YUV420,
};

struct cam_f1c200s_tvd_pdata_t {
	virtual_addr_t virt;
	char * clk;
	int irq;
	int reset;
	int channel;

	unsigned char yc[720 * 576 * 2];
	enum video_format_t fmt;
	int width;
	int height;
	int buflen;
	int ready;
};

static inline void f1c200s_tvd_set_gain(struct cam_f1c200s_tvd_pdata_t * pdat, int gain)
{
	u32_t val;

	val = read32(pdat->virt + TVD_CHROM);
	val &= ~(0xff << 16);
	val |= (gain & 0xff) << 16;
	write32(pdat->virt + TVD_CHROM, val);
}

static inline int f1c200s_tvd_get_gain(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	return (read32(pdat->virt + TVD_CHROM) >> 16) & 0xff;
}

static inline void f1c200s_tvd_set_saturation(struct cam_f1c200s_tvd_pdata_t * pdat, int saturation)
{
	u32_t val;

	val = read32(pdat->virt + TVD_ENHANCE2);
	val &= ~(0xff << 0);
	val |= (saturation & 0xff) << 0;
	write32(pdat->virt + TVD_ENHANCE2, val);
}

static inline int f1c200s_tvd_get_saturation(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	return (read32(pdat->virt + TVD_ENHANCE2) >> 0) & 0xff;
}

static inline void f1c200s_tvd_set_brightness(struct cam_f1c200s_tvd_pdata_t * pdat, int brightness)
{
	u32_t val;

	val = read32(pdat->virt + TVD_ENHANCE1);
	val &= ~(0xff << 8);
	val |= (brightness & 0xff) << 8;
	write32(pdat->virt + TVD_ENHANCE1, val);
}

static inline int f1c200s_tvd_get_brightness(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	return (read32(pdat->virt + TVD_ENHANCE1) >> 8) & 0xff;
}

static inline void f1c200s_tvd_set_contrast(struct cam_f1c200s_tvd_pdata_t * pdat, int contrast)
{
	u32_t val;

	val = read32(pdat->virt + TVD_ENHANCE1);
	val &= ~(0xff << 0);
	val |= (contrast & 0xff) << 0;
	write32(pdat->virt + TVD_ENHANCE1, val);
}

static inline int f1c200s_tvd_get_contrast(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	return (read32(pdat->virt + TVD_ENHANCE1) >> 0) & 0xff;
}

static inline int f1c200s_tvd_get_source(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	u32_t val;

	val = read32(pdat->virt + 0x0e40);
	if((val & 0xe) == 0xe)
	{
		if((val & (1 << 18)) && (val & (1 << 16)))
			return TVD_SOURCE_PAL;
	}
	return TVD_SOURCE_NTSC;
}

static inline void f1c200s_tvd_select_channel(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	u32_t val;

	val = read32(pdat->virt + 0x0e04);
	if(pdat->channel == 0)
		val &= ~(1 << 0);
	else
		val |= (1 << 0);
	write32(pdat->virt + 0x0e04, val);
}

static inline void f1c200s_tvd_set_hstart(struct cam_f1c200s_tvd_pdata_t * pdat, int hstart)
{
	u32_t val;

	val = read32(pdat->virt + 0x001c);
	val &= ~(0xfff << 16);
	val |= (hstart & 0xfff) << 16;
	write32(pdat->virt + 0x001c, val);
}

static inline void f1c200s_tvd_set_vstart(struct cam_f1c200s_tvd_pdata_t * pdat, int vstart)
{
	u32_t val;

	val = read32(pdat->virt + 0x001c);
	val &= ~(0x7ff << 0);
	val |= (vstart & 0x7ff) << 0;
	write32(pdat->virt + 0x001c, val);
}

static inline void f1c200s_tvd_set_width(struct cam_f1c200s_tvd_pdata_t * pdat, int width)
{
	u32_t val;

	val = read32(pdat->virt + 0x008c);
	val &= ~(0xfff << 0);
	val |= ((width > 720) ? 720 : width) << 0;
	write32(pdat->virt + 0x008c, val);
}

static inline void f1c200s_tvd_set_width_jump(struct cam_f1c200s_tvd_pdata_t * pdat, int jump)
{
	write32(pdat->virt + 0x0090, jump);
}

static inline void f1c200s_tvd_set_height(struct cam_f1c200s_tvd_pdata_t * pdat, int height)
{
	u32_t val;

	val = read32(pdat->virt + 0x008c);
	val &= ~(0x7ff << 16);
	val |= height << 16;
	write32(pdat->virt + 0x008c, val);
}

static inline void f1c200s_tvd_set_fmt(struct cam_f1c200s_tvd_pdata_t *pdat, enum tvd_foramt_t fmt)
{
	u32_t val = read32(pdat->virt + 0x0088);

	switch(fmt)
	{
	case TVD_PL_YUV422:
		val &= ~(1 << 24);
		val |= 1 << 4;
		break;
	case TVD_PL_YUV420:
		val &= ~(1 << 24);
		val &= ~(1 << 4);
		break;
	case TVD_MB_YUV420:
		val |= 1 << 24;
		val &= ~(1 << 4);
		break;
	default:
		break;
	}
	write32(pdat->virt + 0x0088, val);
}

static inline void f1c200s_tvd_set_blue(struct cam_f1c200s_tvd_pdata_t * pdat, int flag)
{
	u32_t val;

	val = read32(pdat->virt + TVD_MODE);
	val &= ~(0x3 << 4);
	val |= ((flag & 0x3) << 4);
	write32(pdat->virt + TVD_MODE, val);
}

static inline void f1c200s_tvd_set_addr(struct cam_f1c200s_tvd_pdata_t * pdat, void * y, void * c)
{
	u32_t val;

	write32(pdat->virt + TVD_YADDR, (u32_t)y);
	write32(pdat->virt + TVD_CADDR, (u32_t)c);
	val = read32(pdat->virt + 0x0088);
	val |= 1 << 28;
	write32(pdat->virt + 0x0088, val);
}

static inline void f1c200s_tvd_irq_enable(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	u32_t val;

	val = read32(pdat->virt + TVD_IRQ_CTL);
	val |= 1 << 24;
	write32(pdat->virt + TVD_IRQ_CTL, val);
}

static inline void f1c200s_tvd_irq_disable(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	u32_t val;

	val = read32(pdat->virt + TVD_IRQ_CTL);
	val &= ~(1 << 24);
	write32(pdat->virt + TVD_IRQ_CTL, val);
}

static inline void f1c200s_tvd_irq_clear(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	write32(pdat->virt + TVD_IRQ_STATUS, 1 << 24);
}

static inline void f1c200s_tvd_capture_on(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	u32_t val;

	val = read32(pdat->virt + 0x0088);
	val |= 1 << 0;
	write32(pdat->virt + 0x0088, val);
}

static inline void f1c200s_tvd_capture_off(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	u32_t val;

	val = read32(pdat->virt + 0x0088);
	val &= ~(1 << 0);
	write32(pdat->virt + 0x0088, val);
}

static inline void f1c200s_tvd_config(struct cam_f1c200s_tvd_pdata_t * pdat, enum tvd_source_t s, enum tvd_foramt_t fmt)
{
	if(s == TVD_SOURCE_NTSC)
	{
		write32(pdat->virt + 0x0008, 0x00010001);
		write32(pdat->virt + TVD_ENHANCE1, 0x00202068);
		write32(pdat->virt + TVD_ENHANCE2, 0x00300080);
		write32(pdat->virt + 0x0018, 0x21f07c1f);
		write32(pdat->virt + 0x001c, 0x00820022);
		write32(pdat->virt + 0x0f08, 0x00590100);
		write32(pdat->virt + 0x0f0c, 0x00000010);
		write32(pdat->virt + 0x0f10, 0x008a32dd);
		write32(pdat->virt + TVD_MODE, 0x800000a0);
		write32(pdat->virt + TVD_CHROM, 0x008a0000);
		write32(pdat->virt + 0x0f2c, 0x0000cb74);
		write32(pdat->virt + 0x0f44, 0x00004632);
		write32(pdat->virt + 0x0f74, 0x000003c3);
		write32(pdat->virt + 0x0f80, 0x00500000);
		write32(pdat->virt + 0x0f84, 0x00610000);
		write32(pdat->virt + 0x0000, 0x00000001);
		if(fmt == TVD_MB_YUV420)
		{
			f1c200s_tvd_set_width(pdat, 704);
			f1c200s_tvd_set_width_jump(pdat, 704);
			f1c200s_tvd_set_height(pdat, 224);
			f1c200s_tvd_set_vstart(pdat, 0x22 + (480 - 448) / 2);
		}
		else
		{
			f1c200s_tvd_set_width(pdat, 720);
			f1c200s_tvd_set_width_jump(pdat, 720);
			f1c200s_tvd_set_height(pdat, 240);
		}
	}
	else if(s == TVD_SOURCE_PAL)
	{
		write32(pdat->virt + 0x0008, 0x01101001);
		write32(pdat->virt + TVD_ENHANCE1, 0x00202068);
		write32(pdat->virt + TVD_ENHANCE2, 0x00300050);
		write32(pdat->virt + 0x0018, 0x2a098acb);
		write32(pdat->virt + 0x001c, 0x0087002a);
		write32(pdat->virt + 0x0f08, 0x11590902);
		write32(pdat->virt + 0x0f0c, 0x00000016);
		write32(pdat->virt + 0x0f10, 0x008a32ec);
		write32(pdat->virt + TVD_MODE, 0x800000a0);
		write32(pdat->virt + TVD_CHROM, 0x00930000);
		write32(pdat->virt + 0x0f2c, 0x00000d74);
		write32(pdat->virt + 0x0f44, 0x0000412d);
		write32(pdat->virt + 0x0f74, 0x00000343);
		write32(pdat->virt + 0x0f80, 0x00500000);
		write32(pdat->virt + 0x0f84, 0x00c10000);
		write32(pdat->virt + 0x0000, 0x00000001);
		if(fmt == TVD_MB_YUV420)
		{
			f1c200s_tvd_set_width(pdat, 704);
			f1c200s_tvd_set_width_jump(pdat, 704);
			f1c200s_tvd_set_height(pdat, 288);
		}
		else
		{
			f1c200s_tvd_set_width(pdat, 720);
			f1c200s_tvd_set_width_jump(pdat, 720);
			f1c200s_tvd_set_height(pdat, 288);
		}
	}
}

static inline void f1c200s_tvd_init(struct cam_f1c200s_tvd_pdata_t * pdat)
{
	enum tvd_source_t s;

	write32(pdat->virt + 0x0088, 0x04000000);
	write32(pdat->virt + 0x0070, 0x00000100);
	write32(pdat->virt + 0x0e04, 0x8002aaa8);
	write32(pdat->virt + 0x0e2c, 0x00110000);
	write32(pdat->virt + 0x0040, 0x04000310);
	write32(pdat->virt + 0x0000, 0x00000000);
	mdelay(1);
	write32(pdat->virt + 0x0000, 0x00001f01);
	mdelay(1);
	write32(pdat->virt + 0x0014, 0x20000000);
	write32(pdat->virt + 0x0f24, 0x0682810a);
	write32(pdat->virt + 0x0f28, 0x00006440);
	write32(pdat->virt + 0x0f4c, 0x0e70106c);
	write32(pdat->virt + 0x0f54, 0x00000000);
	write32(pdat->virt + 0x0f58, 0x00000082);
	write32(pdat->virt + 0x0f6c, 0x00fffad0);
	write32(pdat->virt + 0x0f70, 0x0000a010);
	f1c200s_tvd_select_channel(pdat);
	mdelay(50);

	s = f1c200s_tvd_get_source(pdat);
	if(s == TVD_SOURCE_NTSC)
	{
		pdat->fmt = VIDEO_FORMAT_NV12;
		pdat->width = 720;
		pdat->height = 480;
		pdat->buflen = pdat->width * pdat->height * 2;
		pdat->ready = 0;
		f1c200s_tvd_config(pdat, s, TVD_PL_YUV420);
		f1c200s_tvd_set_fmt(pdat, TVD_PL_YUV420);
		f1c200s_tvd_set_addr(pdat, &pdat->yc[0], &pdat->yc[pdat->width * pdat->height]);
	}
	else if(s == TVD_SOURCE_PAL)
	{
		pdat->fmt = VIDEO_FORMAT_NV12;
		pdat->width = 720;
		pdat->height = 576;
		pdat->buflen = pdat->width * pdat->height * 2;
		pdat->ready = 0;
		f1c200s_tvd_config(pdat, s, TVD_PL_YUV420);
		f1c200s_tvd_set_fmt(pdat, TVD_PL_YUV420);
		f1c200s_tvd_set_addr(pdat, &pdat->yc[0], &pdat->yc[pdat->width * pdat->height]);
	}
	f1c200s_tvd_set_blue(pdat, 2);
	f1c200s_tvd_capture_off(pdat);
	f1c200s_tvd_irq_clear(pdat);
	f1c200s_tvd_irq_disable(pdat);
}

static int cam_start(struct camera_t * cam, enum video_format_t fmt, int width, int height)
{
	struct cam_f1c200s_tvd_pdata_t * pdat = (struct cam_f1c200s_tvd_pdata_t *)cam->priv;
	f1c200s_tvd_init(pdat);
	f1c200s_tvd_irq_enable(pdat);
	f1c200s_tvd_irq_clear(pdat);
	f1c200s_tvd_capture_on(pdat);
	return 1;
}

static int cam_stop(struct camera_t * cam)
{
	struct cam_f1c200s_tvd_pdata_t * pdat = (struct cam_f1c200s_tvd_pdata_t *)cam->priv;
	f1c200s_tvd_capture_off(pdat);
	f1c200s_tvd_irq_clear(pdat);
	f1c200s_tvd_irq_disable(pdat);
	return 1;
}

static int cam_capture(struct camera_t * cam, struct video_frame_t * frame)
{
	struct cam_f1c200s_tvd_pdata_t * pdat = (struct cam_f1c200s_tvd_pdata_t *)cam->priv;
	if(pdat->ready)
	{
		frame->fmt = pdat->fmt;
		frame->width = pdat->width;
		frame->height = pdat->height;
		frame->buflen = pdat->buflen;
		frame->buf = pdat->yc;
		pdat->ready = 0;
		return 1;
	}
	return 0;
}

static int cam_ioctl(struct camera_t * cam, const char * cmd, void * arg)
{
	struct cam_f1c200s_tvd_pdata_t * pdat = (struct cam_f1c200s_tvd_pdata_t *)cam->priv;
	int * p = arg;

	switch(shash(cmd))
	{
	case 0x440a6553: /* "camera-set-gain" */
		if(p)
		{
			f1c200s_tvd_set_gain(pdat, *p);
			return 0;
		}
		break;
	case 0x30f46ac7: /* "camera-get-gain" */
		if(p)
		{
			*p = f1c200s_tvd_get_gain(pdat);
			return 0;
		}
		break;
	case 0x8531e7af: /* "camera-set-exposure" */
		break;
	case 0x39c98723: /* "camera-get-exposure" */
		break;
	case 0x2d64af48: /* "camera-set-white-balance" */
		break;
	case 0x4b2e173c: /* "camera-get-white-balance" */
		break;
	case 0xc54c8c54: /* "camera-set-focus" */
		break;
	case 0x4f774048: /* "camera-get-focus" */
		break;
	case 0x7ecdcd0f: /* "camera-set-mirror" */
		break;
	case 0x4e4eff83: /* "camera-get-mirror" */
		break;
	case 0xd5d73dfe: /* "camera-set-saturation" */
		if(p)
		{
			f1c200s_tvd_set_saturation(pdat, *p);
			return 0;
		}
		break;
	case 0x0ed48a72: /* "camera-get-saturation" */
		if(p)
		{
			*p = f1c200s_tvd_get_saturation(pdat);
			return 0;
		}
		break;
	case 0xdae4842d: /* "camera-set-brightness" */
		if(p)
		{
			f1c200s_tvd_set_brightness(pdat, *p);
			return 0;
		}
		break;
	case 0x13e1d0a1: /* "camera-get-brightness" */
		if(p)
		{
			*p = f1c200s_tvd_get_brightness(pdat);
			return 0;
		}
		break;
	case 0xf3916322: /* "camera-set-contrast" */
		if(p)
		{
			f1c200s_tvd_set_contrast(pdat, *p);
			return 0;
		}
		break;
	case 0xa8290296: /* "camera-get-contrast" */
		if(p)
		{
			*p = f1c200s_tvd_get_contrast(pdat);
			return 0;
		}
		break;
	default:
		break;
	}
	return -1;
}

static void f1c200s_tvd_interrupt(void * data)
{
	struct camera_t * cam = (struct camera_t *)data;
	struct cam_f1c200s_tvd_pdata_t * pdat = (struct cam_f1c200s_tvd_pdata_t *)cam->priv;
	pdat->ready = 1;
	f1c200s_tvd_irq_clear(pdat);
}

static struct device_t * cam_f1c200s_tvd_probe(struct driver_t * drv, struct dtnode_t * n)
{
	struct cam_f1c200s_tvd_pdata_t * pdat;
	struct camera_t * cam;
	struct device_t * dev;
	virtual_addr_t virt = phys_to_virt(dt_read_address(n));
	char * clk = dt_read_string(n, "clock-name", NULL);
	int irq = dt_read_int(n, "interrupt", -1);

	if(!search_clk(clk))
		return NULL;

	if(!irq_is_valid(irq))
		return NULL;

	pdat = malloc(sizeof(struct cam_f1c200s_tvd_pdata_t));
	if(!pdat)
		return NULL;

	cam = malloc(sizeof(struct camera_t));
	if(!cam)
	{
		free(pdat);
		return NULL;
	}

	pdat->virt = virt;
	pdat->clk = strdup(clk);
	pdat->irq = irq;
	pdat->reset = dt_read_int(n, "reset", -1);
	pdat->channel = clamp(dt_read_int(n, "channel", 0), 0, 1);
	pdat->ready = 0;

	cam->name = alloc_device_name(dt_read_name(n), -1);
	cam->start = cam_start;
	cam->stop = cam_stop;
	cam->capture = cam_capture;
	cam->ioctl = cam_ioctl;
	cam->priv = pdat;

	clk_enable(pdat->clk);
	if(pdat->reset >= 0)
		reset_deassert(pdat->reset);
	request_irq(pdat->irq, f1c200s_tvd_interrupt, IRQ_TYPE_NONE, cam);

	if(!(dev = register_camera(cam, drv)))
	{
		free_irq(pdat->irq);
		clk_disable(pdat->clk);
		free(pdat->clk);
		free_device_name(cam->name);
		free(cam->priv);
		free(cam);
		return NULL;
	}
	return dev;
}

static void cam_f1c200s_tvd_remove(struct device_t * dev)
{
	struct camera_t * cam = (struct camera_t *)dev->priv;
	struct cam_f1c200s_tvd_pdata_t * pdat = (struct cam_f1c200s_tvd_pdata_t *)cam->priv;

	if(cam)
	{
		unregister_camera(cam);
		free_irq(pdat->irq);
		clk_disable(pdat->clk);
		free(pdat->clk);
		free_device_name(cam->name);
		free(cam->priv);
		free(cam);
	}
}

static void cam_f1c200s_tvd_suspend(struct device_t * dev)
{
}

static void cam_f1c200s_tvd_resume(struct device_t * dev)
{
}

static struct driver_t cam_f1c200s_tvd = {
	.name		= "cam-f1c200s-tvd",
	.probe		= cam_f1c200s_tvd_probe,
	.remove		= cam_f1c200s_tvd_remove,
	.suspend	= cam_f1c200s_tvd_suspend,
	.resume		= cam_f1c200s_tvd_resume,
};

static __init void cam_f1c200s_tvd_driver_init(void)
{
	register_driver(&cam_f1c200s_tvd);
}

static __exit void cam_f1c200s_tvd_driver_exit(void)
{
	unregister_driver(&cam_f1c200s_tvd);
}

driver_initcall(cam_f1c200s_tvd_driver_init);
driver_exitcall(cam_f1c200s_tvd_driver_exit);
