/*****************************************************************//**
 * \file   image_helper.h
 * \brief  Declares class image_base
 * 
 * \author Mikalai Varapai
 * \date   May 2024
 *********************************************************************/
#pragma once

#include <cstdint>

enum IMAGE_COLOR_MODE
{
	IMAGE_COLOR_MODE_RGB = 3,		// RGB - 3 bytes
	IMAGE_COLOR_MODE_GRAYSCALE = 1	// A - 1 byte
};

struct Color3
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/**
 * Class used as a storage of image data and its interpretation to common image formats.
 */
class image_base
{
protected:
	image_base();
	~image_base();
	image_base(image_base& other);


	int read_raw_memory_from_file(const char* src, uint32_t width, uint32_t height, IMAGE_COLOR_MODE mode, int byte_offset = 0);
	int read_raw_memory(void* memory, uint32_t width, uint32_t height, IMAGE_COLOR_MODE mode, int byte_offset = 0);
	int read_bmp(const char* src);
	int write_bmp(const char* dst) const;

	void set_color_mode(IMAGE_COLOR_MODE mode);

protected:
	// Raw image_base memory
	// uninitialized at construction
	void* m_pRaw = nullptr;
	uint32_t m_rawByteSize = 0;

	// image_base dimensions - set in constructor
	uint32_t m_width = 0;
	uint32_t m_height = 0;

	// image_base color mode - set in constructor
	IMAGE_COLOR_MODE m_colorMode = IMAGE_COLOR_MODE_RGB;

	uint32_t m_rowByteSize = 0;

	// Helper methods for subsclasses
	void set_color8(int row, int col, uint8_t val);
	void set_color24(int row, int col, Color3 val);
	uint8_t const get_color8(int row, int col);
	Color3 const get_color24(int row, int col);

protected:
	const char* at(int row, int col);
};

// 8-bit .bmp heightmap
class HeightmapImage : public image_base
{
public:
	HeightmapImage(std::string filename)
	{
		m_rawByteSize = 256 * 256;
		read_bmp(filename.c_str());
	}

	uint8_t GetPixel(int row, int col)
	{
		return get_color8(row, col);
	}

	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }

	void write()
	{
		write_bmp("test.bmp");
	}
};