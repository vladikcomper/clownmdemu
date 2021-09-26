#ifndef VDP_H
#define VDP_H

#include <stddef.h>

#include "clowncommon.h"

#define MAX_SCANLINE_WIDTH 320

typedef struct VDP_State
{
	struct
	{
		cc_bool read_mode;
		unsigned char *selected_buffer;
		size_t selected_buffer_size;
		size_t index;
	} access;

	cc_bool write_pending;
	unsigned short cached_write;

	unsigned char vram[0x10000];
	unsigned char cram[4 * 16 * 2];
	unsigned char vsram[MAX_SCANLINE_WIDTH / 16 * 2];
} VDP_State;

void VDP_Init(VDP_State *state);
void VDP_RenderScanline(VDP_State *state, void (*scanline_rendered_callback)(void *pixels, size_t screen_width, size_t screen_height));

unsigned short VDP_ReadData(VDP_State *state);
unsigned short VDP_ReadControl(VDP_State *state);
void VDP_WriteData(VDP_State *state, unsigned short value);
void VDP_WriteControl(VDP_State *state, unsigned short value);

#endif /* VDP_H */