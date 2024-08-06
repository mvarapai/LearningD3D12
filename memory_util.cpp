/*****************************************************************//**
 * \file   memory_util.cpp
 * \brief  Definition of memory util class
 * 
 * \author Mikalai Varapai
 * \date   May 2024
 *********************************************************************/
#include <memory>

#include "memory_util.h"

/**
 * Create generic_data. Memory is allocated by the user
 * 
 * \param address address of already allocated memory
 * \param size allocation size
 */
generic_data::generic_data(void* address, uint32_t size)
{
	m_size = size;
	m_baseAddress = address;
}

generic_data::~generic_data()
{
}

// Write 8 bits at given offset
void generic_data::write8(uint64_t offset, uint8_t val)
{
	uint8_t* addr = (uint8_t*)((uint64_t)m_baseAddress + offset);
	*addr = val;
}

// Write 16 bits at given offset
void generic_data::write16(uint64_t offset, uint16_t val)
{
	uint16_t* addr = (uint16_t*)((uint64_t)m_baseAddress + offset);
	*addr = val;
}

// Write 32 bits at given offset
void generic_data::write32(uint64_t offset, uint32_t val)
{
	uint32_t* addr = (uint32_t*)((uint64_t)m_baseAddress + offset);
	*addr = val;
}
