#include "eventlog.h"
#include "endianness.h"

#include "string.h"


#define ASSERT_FLASH_INCREMENT_NOT_OK(x)                                                                     \
	if((x) != FLASH_INCREMENT_STATUS_OK)                                                                     \
	{                                                                                                        \
		return EVENTLOG_STATUS_ERROR;                                                                        \
	}


#define ELEMENT_SIZE			 sizeof(eventlog_element_t)
#define ELEMENT_SIZE_WITHOUT_CRC (sizeof(eventlog_element_t) - sizeof(crc_t))
#define ELEMENT_SIZE_TO_READ	 (sizeof(counter_t) + sizeof(type_t) + sizeof(time_t))

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	#error Check behaviour of reading from flash on big endian system, this was not verified.
#endif

typedef struct eventlog_element
{
		counter_t counter;
		type_t type;
		time_t time;
		crc_t crc;
} __attribute__((__packed__)) eventlog_element_t;


static uint16_t crc_calc(const void* const buffer, const size_t size)
{
	const char* const buffer_data = buffer;

	uint16_t crc = 0;

	for(size_t x = 0; x < size; ++x)
	{
		crc += buffer_data[x];
	}

	return crc;
}


eventlog_status_t eventlog_init(eventlog_t* const I, const unsigned pages_number, const unsigned page_size,
								uint8_t empty_flash_value,
								flash_increment_erase_page_t erase_page_events_callback,
								flash_increment_read_page_t read_page_events_callback,
								flash_increment_write_page_t write_page_events_callback,
								flash_increment_erase_page_t erase_page_header_callback,
								flash_increment_read_page_t read_page_header_callback,
								flash_increment_write_page_t write_page_header_callback)
{
	if(I == NULL || pages_number == 0 || page_size == 0)
	{
		return EVENTLOG_STATUS_ERROR;
	}

	if(erase_page_events_callback == NULL || read_page_events_callback == NULL ||
	   write_page_events_callback == NULL)
	{
		return EVENTLOG_STATUS_ERROR;
	}

	if(erase_page_header_callback == NULL || read_page_header_callback == NULL ||
	   write_page_header_callback == NULL)
	{
		return EVENTLOG_STATUS_ERROR;
	}

	// logic of error log assumes that elements to write never exceed page size
	if(page_size % ELEMENT_SIZE != 0)
	{
		return EVENTLOG_STATUS_ERROR;
	}


	I->max_saved_counter_value = 0;
	I->empty_flash_value = empty_flash_value;

	flash_increment_status_t status_flash_increment;
	eventlog_element_t last_written_to_element;
	unsigned max_header_sector_flash_bytes;
	unsigned empty_read_value;
	unsigned read_value;
	unsigned last_offset_written_to = 0;

	memset(&empty_read_value, empty_flash_value, sizeof(empty_read_value));

	status_flash_increment =
		flash_increment_init(&I->flash_increment_events, pages_number, page_size, erase_page_events_callback,
							 read_page_events_callback, write_page_events_callback);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	status_flash_increment =
		flash_increment_init(&I->flash_increment_header, 1, page_size, erase_page_header_callback,
							 read_page_header_callback, write_page_header_callback);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	status_flash_increment =
		flash_increment_get_max_flash_size(&I->flash_increment_header, &max_header_sector_flash_bytes);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	// read max counter-value and its index
	for(int x = 0; x < max_header_sector_flash_bytes; x += sizeof(last_offset_written_to))
	{
		status_flash_increment = flash_increment_set_read_offset(&I->flash_increment_header, x);
		ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

		status_flash_increment =
			flash_increment_read(&I->flash_increment_header, &read_value, sizeof(read_value));
		ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

		if(memcmp(&read_value, &empty_read_value, sizeof(last_offset_written_to)) == 0)
		{
			break;
		}
		{
			last_offset_written_to = read_value;
		}
	}

	status_flash_increment =
		flash_increment_set_read_offset(&I->flash_increment_events, last_offset_written_to);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	status_flash_increment = flash_increment_read(&I->flash_increment_events, &last_written_to_element,
												  sizeof(last_written_to_element));
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	I->max_saved_counter_value = last_written_to_element.counter;

	status_flash_increment =
		flash_increment_set_write_offset(&I->flash_increment_events, last_offset_written_to + ELEMENT_SIZE);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	return EVENTLOG_STATUS_OK;
}

eventlog_status_t eventlog_write(eventlog_t* const I, const type_t type, const time_t time)
{
	flash_increment_status_t status_flash_increment;
	eventlog_element_t element;

	I->max_saved_counter_value += 1;

	element.counter = I->max_saved_counter_value;
	element.type = type;
	element.time = time;
	element.crc = crc_calc(&element, ELEMENT_SIZE_WITHOUT_CRC);

	unsigned last_offset_written_to;
	flash_increment_get_write_offset(&I->flash_increment_events, &last_offset_written_to);

	status_flash_increment = flash_increment_write(&I->flash_increment_header, true, &last_offset_written_to,
												   sizeof(last_offset_written_to));
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	status_flash_increment = flash_increment_write(&I->flash_increment_events, true, &element, ELEMENT_SIZE);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	return EVENTLOG_STATUS_OK;
}

eventlog_status_t eventlog_read(eventlog_t* const I, void* const buffer, size_t* const received_size,
								const size_t max_buffer_size, const size_t number_to_read,
								size_t* const number_of_reads, const size_t offset_of_read)
{
	if(I == NULL || buffer == NULL || received_size == NULL || max_buffer_size == 0 || number_to_read == 0 ||
	   number_of_reads == NULL)
	{
		return EVENTLOG_STATUS_ERROR;
	}

	*received_size = 0;
	*number_of_reads = 0;

	flash_increment_status_t status_flash_increment;
	unsigned max_flash_size;
	unsigned actual_write_position;
	size_t number_to_read_fixed;

	// fixed read element number based on max buffer size
	if(ELEMENT_SIZE_TO_READ * number_to_read > max_buffer_size)
	{
		number_to_read_fixed = max_buffer_size / ELEMENT_SIZE_TO_READ;
	}
	else
	{
		number_to_read_fixed = number_to_read;
	}

	status_flash_increment = flash_increment_get_max_flash_size(&I->flash_increment_events, &max_flash_size);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	// offset is bigger than flash can store
	if(offset_of_read * ELEMENT_SIZE >= max_flash_size)
	{
		return EVENTLOG_STATUS_LOGIC_ERROR;
	}

	// number to read is bigger that flash can store when included user offset
	if(number_to_read_fixed > ((max_flash_size / ELEMENT_SIZE) - offset_of_read))
	{
		number_to_read_fixed = ((max_flash_size / ELEMENT_SIZE) - offset_of_read);
	}

	status_flash_increment =
		flash_increment_get_write_offset(&I->flash_increment_events, &actual_write_position);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	status_flash_increment =
		flash_increment_set_read_offset(&I->flash_increment_events, actual_write_position);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	// move read_position to last write position
	status_flash_increment = flash_increment_move_left_read_offset(&I->flash_increment_events, ELEMENT_SIZE);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	// move left read flash with offset elements
	if(offset_of_read != 0)
	{
		status_flash_increment =
			flash_increment_move_left_read_offset(&I->flash_increment_events, offset_of_read * ELEMENT_SIZE);
		ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);
	}

	for(int x = 0; x < number_to_read_fixed; ++x)
	{
		eventlog_element_t element;
		crc_t calculated_crc;

		status_flash_increment = flash_increment_read(&I->flash_increment_events, &element, ELEMENT_SIZE);
		ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

		calculated_crc = crc_calc(&element, ELEMENT_SIZE_WITHOUT_CRC);

		// return all events received properly from flash
		if(element.crc != calculated_crc)
		{
			return EVENTLOG_STATUS_OK;
		}

		*received_size += ELEMENT_SIZE_TO_READ;
		*number_of_reads += 1;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		uint32_t counter_le = end_be32toh(element.counter);
		uint16_t type_le = end_be16toh(element.type);
		time_t time_le = end_be64toh(element.time);
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	#error Check behaviour of reading on big endian system, this was not verified.
#endif

		memcpy(buffer + (x * ELEMENT_SIZE_TO_READ), &counter_le, sizeof(element.counter));
		memcpy(buffer + (x * ELEMENT_SIZE_TO_READ) + sizeof(element.counter), &type_le, sizeof(element.type));
		memcpy(buffer + (x * ELEMENT_SIZE_TO_READ) + sizeof(element.counter) + sizeof(element.type), &time_le,
			   sizeof(element.time));

		status_flash_increment =
			flash_increment_move_left_read_offset(&I->flash_increment_events, 2 * ELEMENT_SIZE);
		ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);
	}

	return EVENTLOG_STATUS_OK;
}

eventlog_status_t eventlog_erase(eventlog_t* const I)
{
	flash_increment_status_t status_flash_increment;

	status_flash_increment = flash_increment_clear_memory(&I->flash_increment_events);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	status_flash_increment = flash_increment_clear_memory(&I->flash_increment_header);
	ASSERT_FLASH_INCREMENT_NOT_OK(status_flash_increment);

	I->max_saved_counter_value = 0;

	return EVENTLOG_STATUS_OK;
}
