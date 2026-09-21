/**
 * @brief Unit tests for eventlog module.
 */

#include <stdio.h>
#include "eventlog.h"
#include "string.h"
#include "unity.h"


#define PAGES_NUMBER	  10
#define PAGE_SIZE		  256
#define EMPTY_FLASH_VALUE 0xFF

static eventlog_t ctx = {0};

static uint8_t memory_for_events[PAGE_SIZE * PAGES_NUMBER] = {EMPTY_FLASH_VALUE};
static uint8_t memory_for_header[PAGE_SIZE * 1] = {EMPTY_FLASH_VALUE};

static flash_increment_status_t erase_page_events_callback(unsigned int page)
{
	if(page >= PAGES_NUMBER)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}
	memset(&memory_for_events[page * PAGE_SIZE], EMPTY_FLASH_VALUE, PAGE_SIZE);
	return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t read_page_events_callback(unsigned int page, const uint32_t address_offset,
														  char* const buffer, const size_t size)
{
	if(page >= PAGES_NUMBER || address_offset + size > PAGE_SIZE)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}
	memcpy(buffer, &memory_for_events[page * PAGE_SIZE + address_offset], size);
	return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t write_page_events_callback(unsigned int page, const uint32_t address_offset,
														   const char* const buffer, const size_t size)
{
	if(page >= PAGES_NUMBER || address_offset + size > PAGE_SIZE)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}
	memcpy(&memory_for_events[page * PAGE_SIZE + address_offset], buffer, size);
	return FLASH_INCREMENT_STATUS_OK;
}


static flash_increment_status_t erase_page_header_callback(unsigned int page)
{
	if(page >= 1)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}
	memset(&memory_for_header[page * PAGE_SIZE], EMPTY_FLASH_VALUE, PAGE_SIZE);
	return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t read_page_header_callback(unsigned int page, const uint32_t address_offset,
														  char* const buffer, const size_t size)
{
	if(page >= 1 || address_offset + size > PAGE_SIZE)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}
	memcpy(buffer, &memory_for_header[page * PAGE_SIZE + address_offset], size);
	return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t write_page_header_callback(unsigned int page, const uint32_t address_offset,
														   const char* const buffer, const size_t size)
{
	if(page >= 1 || address_offset + size > PAGE_SIZE)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}
	memcpy(&memory_for_header[page * PAGE_SIZE + address_offset], buffer, size);
	return FLASH_INCREMENT_STATUS_OK;
}

/*---------TESTS---------*/

static void example_test(void)
{
  TEST_ASSERT_EQUAL(1, 1);
}

/*---------END OF TESTS---------*/

void setUp()
{
}

void tearDown()
{
}


int main(void)
{
	eventlog_init(&ctx, PAGES_NUMBER, 1, PAGE_SIZE, EMPTY_FLASH_VALUE, erase_page_events_callback,
				  read_page_events_callback, write_page_events_callback, erase_page_header_callback,
				  read_page_header_callback, write_page_header_callback);

	UNITY_BEGIN();
	RUN_TEST(example_test);
	return UNITY_END();
}
