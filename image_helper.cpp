/*****************************************************************//**
 * \file   image_helper.cpp
 * \brief  Definition of class image_base
 * 
 * \author Mikalai Varapai
 * \date   May 2024
 *********************************************************************/
#include <cstdio>
#include <memory>
#include <fstream>
#include <iostream>
#include <cstring>

#include "image_helper.h"
#include "memory_util.h"

static int padded_row_size_bits(uint32_t row_size_bits)
{
    row_size_bits += 0x1F;
    row_size_bits &= ~0x1F;

    return row_size_bits;
}

static int padded_row_size_bytes(uint32_t row_size_bytes)
{
    row_size_bytes += 0x7;
    row_size_bytes &= ~0x7;

    return row_size_bytes;
}

/**
 * image_base constuctor.
 * 
 * \param width width of the image
 * \param height height of the image
 * \param mode color mode of the image
 */
image_base::image_base()
{
}

/**
 * Reads raw color data from file.
 * 
 * \param src path and/or file name
 * \return error code (0 - success, 1 - warning, -1 - error)
 */
int image_base::read_raw_memory_from_file(const char* src, uint32_t width, uint32_t height, IMAGE_COLOR_MODE mode, int byte_offset)
{
    // Set image data
    m_width = width;
    m_height = height;
    m_colorMode = mode;

    // Calculate row size
    // m_colorMode contains the number of bytes per pixel
    m_rowByteSize = padded_row_size_bytes(m_width * m_colorMode);
    m_rawByteSize = m_rowByteSize * m_height;

    // Allocate memory for raw image_base
    m_pRaw = malloc(m_rawByteSize);

    // Create input file stream object
    std::ifstream in;
    in.open(src, std::ios::in | std::ios::binary);

    // Calculate file size
    uint32_t begin = (uint32_t)in.tellg();
    in.seekg(0, std::ios::end);
    uint32_t end = (uint32_t)in.tellg();
    uint32_t size = end - begin;

    int errorCode = 0;

    if (!m_pRaw)
    {
        fprintf(stderr, "Failed to allocate %u bytes\n", m_rawByteSize);
        errorCode = -1;
    }
    if (size > m_rawByteSize)
    {
        fprintf(stderr, "Incorrect dimensions. Image is specified to be %u x %u (%u bytes), but the file constains %u bytes, memory loss possible\n",
            m_width, m_height, m_rawByteSize, size);
        errorCode = -1;
    }
    if (size < m_rawByteSize)
    {
        fprintf(stderr, "Incorrect dimensions: too much memory allocated. Image is specified to be %u x %u (%u bytes), but the file constains %u bytes.\n",
            m_width, m_height, m_rawByteSize, size);
        errorCode = 1;
    }
    else    // If allocation is successful
    {
        // Reset data pointer and read the file
        in.seekg(byte_offset);                      // In case byte offset specified
        in.read((char*)m_pRaw, m_rawByteSize);
    }
    in.close();
    return errorCode;
}

int image_base::read_raw_memory(void* memory, uint32_t width, uint32_t height, IMAGE_COLOR_MODE mode, int byte_offset)
{
    // Set image data
    m_width = width;
    m_height = height;
    m_colorMode = mode;

    // Calculate row size
    // m_colorMode contains the number of bytes per pixel
    m_rowByteSize = padded_row_size_bytes(m_width * m_colorMode);
    m_rawByteSize = m_rowByteSize * m_height;

    // Allocate memory for raw image_base
    m_pRaw = malloc(m_rawByteSize);

    if (!m_pRaw)
    {
        fprintf(stderr, "Failed to allocate %u bytes\n", m_rawByteSize);
        return -1;
    }

    memcpy(m_pRaw, memory, m_rawByteSize);
    return 0;
}

/**
 * Reads image data from a .bmp file. All data (size, pixel format) is taken from BMP header.
 * 
 * \param src name of the file
 * \return error code (0 - success, 1 - warning, -1 - error)
 */
int image_base::read_bmp(const char* src)
{
    // Read the header
    std::ifstream in;
    in.open(src, std::ios::binary | std::ios::in);

    uint32_t* ptr = new uint32_t();

    // Read m_headerByteSize
    in.seekg(0xA);
    in.read((char*)ptr, 4);
    uint32_t headerByteSize = *ptr;

    // Read m_rawByteSize
    /*in.seekg(0x22);
    in.read((char*)ptr, 4);
    m_rawByteSize = *ptr;*/

    // Read width and height
    in.seekg(0x12);
    in.read((char*)ptr, 4);
    m_width = *ptr;

    in.read((char*)ptr, 4);
    m_height = *ptr;

    // Read color mode
    in.seekg(0x1C);
    in.read((char*)ptr, 2);
    m_colorMode = (IMAGE_COLOR_MODE)(*ptr / 8);

    // Get row size
    m_rowByteSize = padded_row_size_bytes(m_width * m_colorMode);

    delete ptr;

    // Allocate memory
    m_pRaw = malloc(m_rawByteSize);

    in.seekg(headerByteSize);
    in.read((char*)m_pRaw, m_rawByteSize);

    in.close();

    return 0;
}

/**
 * Convert raw color data to BMP and write to file.
 * 
 * \param dst path and/or file name
 * \return error code (0 - success, -1 - error)
 */
int image_base::write_bmp(const char* dst) const
{
    uint32_t headerByteSize = 0x36;

    // If using grayscale mode (8-bit colors), palette table is needed
    if (m_colorMode == IMAGE_COLOR_MODE_GRAYSCALE) headerByteSize += 0x400;

    void* pHeader = malloc(headerByteSize);

    if (!pHeader)
    {
        fprintf(stderr, "Error allocating memory for header\n");
        return -1;
    }

    generic_data header(pHeader, headerByteSize);

    // Start of BMP header
    header.write16(0x0, 0x4D42);                            // "BM"
    header.write32(0x2, headerByteSize + m_rawByteSize);    // Size of BMP file
    header.write32(0x6, 0u);                                // Reserved
    header.write32(0xA, headerByteSize);                    // Start of pixel array

    // Start of DIB header
    header.write32(0xE, 0x28u);                             // Number of bytes in the header
    header.write32(0x12, m_width);                          // Width
    header.write32(0x16, m_height);                         // Height
    header.write16(0x1A, 1u);                               // Number of color planes (ignored)
    header.write16(0x1C, (uint16_t)m_colorMode * 8u);       // Bits per pixel
    header.write32(0x1E, 0u);                               // Compression method
    header.write32(0x22, m_rawByteSize);                    // Size of raw image data
    header.write32(0x26, 0xB13u);                           // Horizontal px/m
    header.write32(0x2A, 0xB13u);                           // Vertical px/m

    uint32_t paletteCount = 0x0u;
    if (m_colorMode == IMAGE_COLOR_MODE_GRAYSCALE) paletteCount = 0xffu;

    header.write32(0x2E, paletteCount);                     // Number of colors in the palette                
    header.write32(0x32, paletteCount);                     // Number of important colors

    // Set the palette for grayscale mode
    if (m_colorMode == IMAGE_COLOR_MODE_GRAYSCALE)
    {
        uint64_t offset = 0x36u;
        for (int i = 0; i < 256; i++)
        {
            
            uint32_t color = 0;
            color += i;
            color += i << 8;
            color += i << 16;
            color += 0xFF << 24;
            header.write32(offset, color);                  // Color (i, i, i, 255) in the palette
            offset += 0x4;
        }
    }

    // Open the file
    std::ofstream out;
    out.open(dst, std::ios::out | std::ios::binary);

    // Write the header
    out.write((const char*)pHeader, headerByteSize);

    // Write the contents
    out.write((const char*)m_pRaw, m_rawByteSize);

    out.close();

    free(pHeader);

    return 0;
}

/**
 * Change image color mode. Raw data is recreated
 * 
 * \param mode image color mode to change
 */
void image_base::set_color_mode(IMAGE_COLOR_MODE mode)
{
    // If the mode is not changed, return
    if (mode == m_colorMode) return;

    // If there is nothing to change, return
    if (m_pRaw == nullptr) return;

    uint32_t newRowByteSize = padded_row_size_bytes(m_width * (uint32_t)mode);
    uint32_t newRawByteSize = newRowByteSize * m_height;

    // Allocate memory for new raw color data
    void* pNewRaw = malloc(newRawByteSize);

    for (uint32_t row = 0; row < m_height; row++)
    {
        static void* currentRow = pNewRaw;
        for (uint32_t col = 0; col < m_width; col++)
        {
            // Static variable declarations
            static Color3 clr24;
            static uint8_t clr8;

            switch (mode)
            {
            case IMAGE_COLOR_MODE_GRAYSCALE:
                // Calculate grayscale color as mean of other colors
                clr24 = get_color24(row, col);
                clr8 = (clr24.r + clr24.g + clr24.b) / 3;
                *(uint8_t*)((uint64_t)currentRow + col) = clr8;
                break;
            case IMAGE_COLOR_MODE_RGB:
                clr8 = get_color8(row, col);
                clr24 = { clr8, clr8, clr8 };
                *(Color3*)((uint64_t)currentRow + (uint64_t)col * 3ull) = clr24;
                break;
            }

        }
        currentRow = (void*)((uint64_t)currentRow + newRowByteSize);
    }

    // Release current raw memory
    free(m_pRaw);

    // Set the class variables
    m_colorMode = mode;
    m_pRaw = pNewRaw;
    m_rawByteSize = newRawByteSize;
    m_rowByteSize = newRowByteSize;
}

void image_base::set_color8(int row, int col, uint8_t val)
{
    if (m_colorMode != IMAGE_COLOR_MODE_GRAYSCALE)
    {
        fprintf(stderr, "Using set_color8 while the image is not grayscale. Avoid using set_color8 with wrong image type.");
    }
    *(uint8_t*)at(row, col) = val;
}

void image_base::set_color24(int row, int col, Color3 val)
{
    if (m_colorMode != IMAGE_COLOR_MODE_RGB)
    {
        fprintf(stderr, "Using set_color24 while the image is not RGB. Avoid using set_color24 with wrong image type.");
    }
    *(Color3*)at(row, col) = val;
}

uint8_t const image_base::get_color8(int row, int col)
{
    if (m_colorMode != IMAGE_COLOR_MODE_GRAYSCALE)
    {
        fprintf(stderr, "Using get_color8 while the image is not grayscale. Avoid using get_color8 with wrong image type.");
    }
    return *(uint8_t*)at(row, col);
}

Color3 const image_base::get_color24(int row, int col)
{
    if (m_colorMode != IMAGE_COLOR_MODE_RGB)
    {
        fprintf(stderr, "Using get_color24 while the image is not grayscale. Avoid using get_color24 with wrong image type.");
    }
    return *(Color3*)at(row, col);
}

const char* image_base::at(int row, int col)
{
    const char* result = (const char*)m_pRaw;
    result += row * m_rowByteSize + col * (uint32_t)m_colorMode;
    return result;
}

image_base::~image_base()
{
    if (m_pRaw) free(m_pRaw);
}

/**
 * Copy constructor. Reinitializes any memory the class has to
 * avoid having two instances pointing to the same memory.
 * 
 * \param other
 */
image_base::image_base(image_base& other)
{
    if (m_pRaw)
    {
        other.m_pRaw = malloc(m_rawByteSize);
        memcpy(other.m_pRaw, m_pRaw, m_rawByteSize);
    }
}
