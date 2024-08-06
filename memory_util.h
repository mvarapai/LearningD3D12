/*****************************************************************//**
 * \file   memory_util.h
 * \brief  Defines class for writing to generic memory
 * 
 * \author Mikalai Varapai
 * \date   May 2024
 *********************************************************************/
#pragma once

#include <cstdint>

/**
 * Class representing generic memory chunk with functions for writing data at given offset.
 */
class generic_data
{
public:
	generic_data(void* address, uint32_t size);		// If memory is intended to be used somewhere else
	~generic_data();

	// Delete default and copy constructors
	generic_data() = delete;
	generic_data(generic_data& other) = delete;

	// Binary writing methods
	void write8(uint64_t offset, uint8_t val);
	void write16(uint64_t offset, uint16_t val);
	void write32(uint64_t offset, uint32_t val);

	// Getter for the data pointer
	void* ptr() { return m_baseAddress; }

private:
	uint32_t m_size = 0u;							// Binary size
	void* m_baseAddress = nullptr;					// Either allocated by class or user
};
