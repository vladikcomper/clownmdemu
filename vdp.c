#include "vdp.h"

#include <stddef.h>

#include "error.h"

void VDP_Init(VDP_State *state)
{
	state->access.write_pending = cc_false;

	state->access.read_mode = cc_false;
	state->access.selected_buffer = state->vram;
	state->access.selected_buffer_size_mask = sizeof(state->vram) / sizeof(unsigned short) - 1;
	state->access.index = 0;
	state->access.increment = 0;

	state->dma.awaiting_destination_address = cc_false;
	state->dma.source_address = 0;

	state->plane_a_address = 0;
	state->plane_b_address = 0;
	state->window_address = 0;
	state->sprite_table_address = 0;
	state->hscroll_address = 0;

	state->plane_width = 32;
	state->plane_height = 32;
	state->plane_width_bitmask = state->plane_width * 8 - 1;
	state->plane_height_bitmask = state->plane_height * 8 - 1;

	state->screen_width = 320;
	state->screen_height = 224;

	state->hscroll_mode = VDP_HSCROLL_MODE_FULL;
	state->vscroll_mode = VDP_VSCROLL_MODE_FULL;

	/* Initialise VDP lookup table */
	/* TODO */
}

void VDP_RenderScanline(VDP_State *state, size_t scanline, void (*scanline_rendered_callback)(size_t scanline, void *pixels, size_t screen_width, size_t screen_height))
{
	unsigned char pixels[MAX_SCANLINE_WIDTH * 2];
	size_t i;

	for (i = 0; i < state->screen_width; ++i)
	{
		size_t hscroll, vscroll;
		size_t plane_x_in_pixels, plane_y_in_pixels;
		size_t plane_x_in_tiles, plane_y_in_tiles;
		size_t tile_x, tile_y;
		unsigned short tile_metadata;
		unsigned short tile_data;
		size_t colour_index;

		switch (state->hscroll_mode)
		{
			case VDP_HSCROLL_MODE_FULL:
				hscroll = state->vram[state->hscroll_address];
				break;

			case VDP_HSCROLL_MODE_1CELL:
				hscroll = state->vram[state->hscroll_address + (scanline / 8) * 2];
				break;

			case VDP_HSCROLL_MODE_1LINE:
				hscroll = state->vram[state->hscroll_address + scanline * 2];
				break;
		}

		switch (state->vscroll_mode)
		{
			case VDP_VSCROLL_MODE_FULL:
				vscroll = state->vsram[0];
				break;

			case VDP_VSCROLL_MODE_2CELL:
				vscroll = state->vsram[(i / 16) * 2];
				break;
		}

		plane_x_in_pixels = hscroll + (i * 8);
		plane_y_in_pixels = vscroll + scanline;

		plane_x_in_tiles = (plane_x_in_pixels / 8) & state->plane_width_bitmask;
		plane_y_in_tiles = (plane_y_in_pixels / 8) & state->plane_height_bitmask;

		tile_metadata = state->vram[state->plane_a_address + plane_y_in_tiles * state->plane_width + plane_x_in_tiles];

		tile_x = plane_x_in_pixels & 7;
		tile_y = plane_y_in_pixels & 7;

		tile_data = state->vram[(tile_metadata & 0x7FFF) + tile_y * 2 + tile_x / 4];

		colour_index = (tile_data >> (4 * (tile_x & 3))) & 0xF;

		pixels[i * 2 + 1] = 0;
		pixels[i * 2 + 1] = colour_index << 2;
	}

	scanline_rendered_callback(scanline, pixels, state->screen_width, state->screen_height);
}

unsigned short VDP_ReadData(VDP_State *state)
{
	unsigned short value = 0;

	if (!state->access.read_mode)
	{
		/* TODO - If I remember right, real Mega Drives crash when this happens */
		PrintError("Data was read from the VDP data port while the VDP was in write mode");
	}
	else
	{
		value = state->access.selected_buffer[state->access.index & state->access.selected_buffer_size_mask];

		state->access.index += state->access.increment;
	}

	return value;
}

unsigned short VDP_ReadControl(VDP_State *state)
{
	/* TODO */

	/* Reading from the control port aborts the VDP waiting for a second command word to be written.
	   This doesn't appear to be documented in the official SDK manuals we have, but the official
	   boot code makes use of this features. */
	state->access.write_pending = cc_false;

	/* Not sure about this though */
	state->dma.awaiting_destination_address = cc_false;

	return 0;
}

void VDP_WriteData(VDP_State *state, unsigned short value)
{
	if (state->access.read_mode)
	{
		PrintError("Data was written to the VDP data port while the VDP was in read mode");
	}
	else
	{
		state->access.selected_buffer[state->access.index & state->access.selected_buffer_size_mask] = value;

		state->access.index += state->access.increment;
	}
}

void VDP_WriteControl(VDP_State *state, unsigned short value, unsigned short (*read_callback)(void *user_data, unsigned long address), void *user_data)
{
	if (state->access.write_pending)
	{
		/* This command is setting up for a memory access (part 2) */
		const unsigned short destination_address = (state->access.cached_write & 0x3FFF) | ((value & 3) << 14);
		unsigned char access_mode = ((state->access.cached_write >> 12) & 3) | ((value >> 2) & 0x3C);

		if (state->dma.awaiting_destination_address)
			access_mode &= 7;

		state->access.write_pending = cc_false;

		state->access.index = destination_address;
		state->access.read_mode = (state->access.cached_write & 0xC000) == 0;

		switch (access_mode)
		{
			case 0: /* VRAM read */
			case 1: /* VRAM write */
				state->access.selected_buffer = state->vram;
				state->access.selected_buffer_size_mask = sizeof(state->vram) / sizeof(unsigned short) - 1;
				break;

			case 8: /* CRAM read */
			case 3: /* CRAM write */
				state->access.selected_buffer = state->cram;
				state->access.selected_buffer_size_mask = sizeof(state->cram) / sizeof(unsigned short) - 1;
				break;

			case 4: /* VSRAM read */
			case 5: /* VSRAM write */
				state->access.selected_buffer = state->vsram;
				state->access.selected_buffer_size_mask = sizeof(state->vsram) / sizeof(unsigned short) - 1;
				break;

			default: /* Invalid */
				PrintError("Invalid VDP access mode specified (0x%X)", access_mode);
				break;
		}

		if (state->dma.awaiting_destination_address)
		{
			/* Firing DMA */
			unsigned short i;

			state->dma.awaiting_destination_address = cc_false;

			switch (state->dma.mode)
			{
				case VDP_DMA_MODE_MEMORY_TO_VRAM:
					for (i = 0; i < state->dma.length; ++i)
						state->access.selected_buffer[(destination_address + i) & state->access.selected_buffer_size_mask] = read_callback(user_data, (state->dma.source_address & 0xFFFF0000) | ((state->dma.source_address + i * 2) & 0xFFFF));

					break;

				case VDP_DMA_MODE_FILL:
					/* TODO */
					break;

				case VDP_DMA_MODE_COPY:
					/* TODO */
					break;
			}
		}
	}
	else if ((value & 0xC000) != 0x8000)
	{
		/* This command is setting up for a memory access (part 1) */
		state->access.write_pending = cc_true;
		state->access.cached_write = value;
	}
	else
	{
		const unsigned char reg = (value >> 8) & 0x3F;
		const unsigned char data = value & 0xFF;

		/* This command is setting a register */
		switch (reg)
		{
			case 0:
				/* MODE SET REGISTER NO.1 */
				/* TODO */
				break;

			case 1:
				/* MODE SET REGISTER NO.2 */
				/* TODO */
				break;

			case 2:
				/* PATTERN NAME TABLE BASE ADDRESS FOR SCROLL A */
				state->plane_a_address = (size_t)(data & 0x38) << (10 - 1);
				break;

			case 3:
				/* PATTERN NAME TABLE BASE ADDRESS FOR WINDOW */
				state->window_address = (size_t)(data & 0x3E) << (10 - 1);
				break;

			case 4:
				/* PATTERN NAME TABLE BASE ADDRESS FOR SCROLL B */
				state->plane_b_address = (size_t)(data & 7) << (13 - 1);
				break;

			case 5:
				/* SPRITE ATTRIBUTE TABLE BASE ADDRESS */
				state->sprite_table_address = (size_t)(data & 0x7F) << (9 - 1);
				break;

			case 7:
				/* BACKGROUND COLOR */
				/* TODO */
				break;

			case 10:
				/* H INTERRUPT REGISTER */
				/* TODO */
				break;

			case 11:
				/* MODE SET REGISTER NO.3 */

				/* TODO - External interrupt */

				state->vscroll_mode = data & 4 ? VDP_VSCROLL_MODE_2CELL : VDP_VSCROLL_MODE_FULL;

				switch (data & 3)
				{
					case 0:
						state->hscroll_mode = VDP_HSCROLL_MODE_FULL;
						break;

					case 1:
						PrintError("Prohibitied H-scroll mode selected");
						break;

					case 2:
						state->hscroll_mode = VDP_HSCROLL_MODE_1CELL;
						break;

					case 3:
						state->hscroll_mode = VDP_HSCROLL_MODE_1LINE;
						break;
				}

				break;

			case 12:
				/* MODE SET REGISTER NO.4 */
				/* TODO */
				break;

			case 13:
				/* H SCROLL DATA TABLE BASE ADDRESS */
				state->hscroll_address = (size_t)(data & 0x3F) << (10 - 1);
				break;

			case 15:
				/* AUTO INCREMENT DATA */
				state->access.increment = data / 2;

				if (data & 1)
					PrintError("Odd VDP increment delta specified");

				break;

			case 16:
			{
				/* SCROLL SIZE */
				const unsigned char width_index  = (data >> 0) & 3;
				const unsigned char height_index = (data >> 4) & 3;

				if ((width_index == 3 && height_index != 0) || (height_index == 3 && width_index != 0))
				{
					PrintError("Selected plane size exceeds 64x64/32x128/128x32");
				}
				else
				{
					switch (width_index)
					{
						case 0:
							state->plane_width = 32;
							break;

						case 1:
							state->plane_width = 64;
							break;

						case 2:
							PrintError("Prohibited plane width selected");
							break;

						case 3:
							state->plane_width = 128;
							break;
					}

					switch (height_index)
					{
						case 0:
							state->plane_height = 32;
							break;

						case 1:
							state->plane_height = 64;
							break;

						case 2:
							PrintError("Prohibited plane height selected");
							break;

						case 3:
							state->plane_height = 128;
							break;
					}

					state->plane_width_bitmask = state->plane_width * 8 - 1;
					state->plane_height_bitmask = state->plane_height * 8 - 1;
				}

				break;
			}

			case 17:
				/* WINDOW H POSITION */
				/* TODO */
				break;

			case 18:
				/* WINDOW V POSITION */
				/* TODO */
				break;

			case 19:
				/* DMA LENGTH COUNTER LOW */
				state->dma.length &= ~(0xFF << 0);
				state->dma.length |= data << 0;
				break;

			case 20:
				/* DMA LENGTH COUNTER HIGH */
				state->dma.length &= ~(0xFF << 8);
				state->dma.length |= data << 8;
				break;

			case 21:
				/* DMA SOURCE ADDRESS LOW */
				state->dma.source_address &= ~(0xFF << 0);
				state->dma.source_address |= data << 0;
				break;

			case 22:
				/* DMA SOURCE ADDRESS MID. */
				state->dma.source_address &= ~(0xFF << 8);
				state->dma.source_address |= data << 8;
				break;

			case 23:
				/* DMA SOURCE ADDRESS HIGH */
				state->dma.source_address &= ~(0xFF << 16);

				if (data & 0x80)
				{
					state->dma.source_address |= (data & 0x7F) << 16;
					state->dma.mode = VDP_DMA_MODE_MEMORY_TO_VRAM;
				}
				else
				{
					state->dma.source_address |= (data & 0x3F) << 16;
					state->dma.mode = data & 0x40 ? VDP_DMA_MODE_COPY : VDP_DMA_MODE_FILL;
				}

				state->dma.awaiting_destination_address = cc_true;

				break;

			case 6:
			case 8:
			case 9:
			case 14:
				/* Unused legacy register */
				break;

			default:
				/* Invalid */
				PrintError("Attempted to set invalid VDP register (0x%X)", reg);
				break;
		}
	}
}
