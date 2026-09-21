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

static uint8_t memory_for_events[PAGE_SIZE * PAGES_NUMBER];
static uint8_t memory_for_header[PAGE_SIZE * 1];

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

static void eventlog_init_rejects_invalid_arguments_test(void)
{
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_ERROR,
					  eventlog_init(NULL, PAGES_NUMBER, 1, PAGE_SIZE, EMPTY_FLASH_VALUE,
									erase_page_events_callback, read_page_events_callback,
									write_page_events_callback, erase_page_header_callback,
									read_page_header_callback, write_page_header_callback));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_ERROR,
					  eventlog_init(&ctx, 0, 1, PAGE_SIZE, EMPTY_FLASH_VALUE,
									erase_page_events_callback, read_page_events_callback,
									write_page_events_callback, erase_page_header_callback,
									read_page_header_callback, write_page_header_callback));
}

static void empty_eventlog_returns_no_events_test(void)
{
	uint8_t buffer[32] = {0};
	size_t received_size = 99;
	size_t number_of_reads = 99;

	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK,
					  eventlog_read(&ctx, buffer, &received_size, sizeof(buffer), 1,
									&number_of_reads, 0));

	TEST_ASSERT_EQUAL(0, received_size);
	TEST_ASSERT_EQUAL(0, number_of_reads);
}

static void standard_event_round_trip_test(void)
{
	const type_t type = 1;
	const time_t timestamp = 123456;
	uint8_t buffer[32] = {0};
	size_t received_size = 0;
	size_t number_of_reads = 0;
	counter_t counter = 0;
	type_t read_type = 0;
	time_t read_timestamp = 0;

	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_write(&ctx, type, timestamp, NULL, 0));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK,
					  eventlog_read(&ctx, buffer, &received_size, sizeof(buffer), 1,
									&number_of_reads, 0));

	memcpy(&counter, buffer, sizeof(counter));
	memcpy(&read_type, buffer + sizeof(counter), sizeof(read_type));
	memcpy(&read_timestamp, buffer + sizeof(counter) + sizeof(read_type), sizeof(read_timestamp));

	TEST_ASSERT_EQUAL(14, received_size);
	TEST_ASSERT_EQUAL(1, number_of_reads);
	TEST_ASSERT_EQUAL(1, counter);
	TEST_ASSERT_EQUAL(type, read_type);
	TEST_ASSERT_EQUAL(timestamp, read_timestamp);
}

static void extended_events_are_returned_newest_first_test(void)
{
	const uint8_t payload[] = {0x10, 0x20, 0x30};
	uint8_t buffer[64] = {0};
	size_t received_size = 0;
	size_t number_of_reads = 0;
	counter_t counter = 0;
	type_t type = 0;
	uint8_t payload_size = 0;

	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_write(&ctx, 1, 10, NULL, 0));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_write(&ctx, 2, 20, payload, sizeof(payload)));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK,
					  eventlog_read(&ctx, buffer, &received_size, sizeof(buffer), 2,
									&number_of_reads, 0));

	memcpy(&counter, buffer, sizeof(counter));
	memcpy(&type, buffer + sizeof(counter), sizeof(type));
	memcpy(&payload_size, buffer + 14, sizeof(payload_size));

	TEST_ASSERT_EQUAL(36, received_size);
	TEST_ASSERT_EQUAL(2, number_of_reads);
	TEST_ASSERT_EQUAL(2, counter);
	TEST_ASSERT_EQUAL(2, type);
	TEST_ASSERT_EQUAL(7, payload_size);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, buffer + 15, sizeof(payload));
	TEST_ASSERT_EQUAL_UINT8(0, buffer[18]);
}

static void read_offset_skips_newest_event_test(void)
{
	uint8_t buffer[32] = {0};
	size_t received_size = 0;
	size_t number_of_reads = 0;
	counter_t counter = 0;

	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_write(&ctx, 1, 10, NULL, 0));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_write(&ctx, 3, 20, NULL, 0));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK,
					  eventlog_read(&ctx, buffer, &received_size, sizeof(buffer), 1,
									&number_of_reads, 1));

	memcpy(&counter, buffer, sizeof(counter));
	TEST_ASSERT_EQUAL(14, received_size);
	TEST_ASSERT_EQUAL(1, number_of_reads);
	TEST_ASSERT_EQUAL(1, counter);
}

static void erase_removes_events_and_resets_counter_test(void)
{
	uint8_t buffer[32] = {0};
	size_t received_size = 0;
	size_t number_of_reads = 0;
	counter_t counter = 0;

	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_write(&ctx, 1, 10, NULL, 0));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_erase(&ctx));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK,
					  eventlog_read(&ctx, buffer, &received_size, sizeof(buffer), 1,
									&number_of_reads, 0));

	TEST_ASSERT_EQUAL(0, received_size);
	TEST_ASSERT_EQUAL(0, number_of_reads);
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK, eventlog_write(&ctx, 1, 10, NULL, 0));
	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK,
					  eventlog_read(&ctx, buffer, &received_size, sizeof(buffer), 1,
									&number_of_reads, 0));
	memcpy(&counter, buffer, sizeof(counter));
	TEST_ASSERT_EQUAL(1, counter);
}

/*---------END OF TESTS---------*/

void setUp()
{
	memset(memory_for_events, EMPTY_FLASH_VALUE, sizeof(memory_for_events));
	memset(memory_for_header, EMPTY_FLASH_VALUE, sizeof(memory_for_header));
	memset(&ctx, 0, sizeof(ctx));

	TEST_ASSERT_EQUAL(EVENTLOG_STATUS_OK,
					  eventlog_init(&ctx, PAGES_NUMBER, 1, PAGE_SIZE, EMPTY_FLASH_VALUE,
									erase_page_events_callback, read_page_events_callback,
									write_page_events_callback, erase_page_header_callback,
									read_page_header_callback, write_page_header_callback));
}

void tearDown()
{
}


int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(eventlog_init_rejects_invalid_arguments_test);
	RUN_TEST(empty_eventlog_returns_no_events_test);
	RUN_TEST(standard_event_round_trip_test);
	RUN_TEST(extended_events_are_returned_newest_first_test);
	RUN_TEST(read_offset_skips_newest_event_test);
	RUN_TEST(erase_removes_events_and_resets_counter_test);
	return UNITY_END();
}
