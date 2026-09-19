#include "eventlog.h"
#include "endianness.h"
#include "string.h"

/*---------------------------------------------------*/

#define ASSERT_FLASH_INCREMENT_NOT_OK(x)                                                                     \
	if((x) != FLASH_INCREMENT_STATUS_OK)                                                                     \
	{                                                                                                        \
		return EVENTLOG_STATUS_ERROR;                                                                        \
	}

#define ASSERT_EVENTLOG_STATUS_NOT_OK(x)                                                                     \
	if((x) != EVENTLOG_STATUS_OK)                                                                            \
	{                                                                                                        \
		return EVENTLOG_STATUS_ERROR;                                                                        \
	}

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	#error Check behaviour of reading from flash on big endian system, this was not verified.
#endif

/**
 * @brief This defined how many bytes will be scanned to search for possibly correct header, when damaged
 * event is encountered.
 */
#define EVENTLOG_MAX_RECOVERY_TRIES_NB 32

/*---------------------------------------------------*/

/**
 * @brief Definitions of eventlog element types. Used to get size
 */

typedef struct
{
		time_t timestamp;
		type_t type;
		counter_t counter;
} __attribute__((__packed__)) eventlog_element_hdr_t;

typedef struct
{
		crc_t crc;
		eventlog_element_hdr_t header;
} __attribute__((__packed__)) eventlog_element_t;

typedef struct
{
		crc_t crc;
		uint8_t payload[EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE];
		uint8_t payload_size;
		eventlog_element_hdr_t header;
} __attribute__((__packed__)) eventlog_element_extended_t;

/*---------------------------------------------------*/

static uint16_t eventlog_crc_calc(const void* const buffer, const size_t size)
{
	const char* const buffer_data = buffer;

	uint16_t crc = 0;

	for(size_t x = 0; x < size; ++x)
	{
		crc += buffer_data[x];
	}

	return crc;
}

static bool eventlog_is_event_extended(const type_t type)
{
	return (type & 0x01) == 0x00 ? true : false;
}

eventlog_status_t eventlog_read_back(flash_increment_t* const I, void* const data, const size_t size)
{
	if(flash_increment_move_left_read_offset(I, size) != FLASH_INCREMENT_STATUS_OK)
	{
		return EVENTLOG_STATUS_ERROR;
	}
	if(flash_increment_read(I, data, size) != FLASH_INCREMENT_STATUS_OK)
	{
		return EVENTLOG_STATUS_ERROR;
	}
	return EVENTLOG_STATUS_OK;
}

static eventlog_status_t eventlog_read_back_and_move_left(flash_increment_t* const I, void* const data,
														  const size_t size)
{
	if(flash_increment_move_left_read_offset(I, size) != FLASH_INCREMENT_STATUS_OK)
	{
		return EVENTLOG_STATUS_ERROR;
	}
	if(flash_increment_read(I, data, size) != FLASH_INCREMENT_STATUS_OK)
	{
		return EVENTLOG_STATUS_ERROR;
	}
	if(flash_increment_move_left_read_offset(I, size) != FLASH_INCREMENT_STATUS_OK)
	{
		return EVENTLOG_STATUS_ERROR;
	}
	return EVENTLOG_STATUS_OK;
}

/* TODO: This is not robust method. Add seperate CRC for header / standard event, and separate CRC for payload
 */
static bool eventlog_is_junk_in_header(const eventlog_element_hdr_t* const header,
									   uint32_t const last_correct_counter)
{
	const uint32_t max_expected_missed_events =
		((EVENTLOG_MAX_RECOVERY_TRIES_NB * 8) / sizeof(eventlog_element_t));
	const uint64_t max_unix_timestamp = 3155756400; /* 01-01-2070 00:00:00 */
	int64_t min_expected_event_counter =
		(int64_t) ((int64_t) last_correct_counter - max_expected_missed_events);

	if(header->counter < last_correct_counter && header->counter > min_expected_event_counter &&
	   header->timestamp < max_unix_timestamp && header->timestamp != 0 && header->type > 0x0000 &&
	   header->type < 0xFFFF)
	{
		return false;
	}
	else
	{
		return true;
	}
}
/*---------------------------------------------------*/

eventlog_status_t eventlog_init(eventlog_t* const I, const unsigned pages_number_events,
								const unsigned pages_number_header, const unsigned page_size,
								uint8_t empty_flash_value,
								flash_increment_erase_page_t erase_page_events_callback,
								flash_increment_read_page_t read_page_events_callback,
								flash_increment_write_page_t write_page_events_callback,
								flash_increment_erase_page_t erase_page_header_callback,
								flash_increment_read_page_t read_page_header_callback,
								flash_increment_write_page_t write_page_header_callback)
{
	eventlog_element_hdr_t last_hdr = {0};
	unsigned hdr_sector_bytes = 0;
	unsigned val_empty = 0;
	unsigned val_read = 0;
	unsigned next_write_offset = 0;
	int nb_of_bytes_read = 0;

	if(I == NULL || pages_number_events == 0 || pages_number_header == 0 || page_size == 0 ||
	   erase_page_events_callback == NULL || read_page_events_callback == NULL ||
	   write_page_events_callback == NULL || erase_page_header_callback == NULL ||
	   read_page_header_callback == NULL || write_page_header_callback == NULL)
	{
		return EVENTLOG_STATUS_ERROR;
	}

	I->empty_flash_value = empty_flash_value;

	memset(&val_empty, I->empty_flash_value, sizeof(unsigned));

	ASSERT_FLASH_INCREMENT_NOT_OK(flash_increment_init(
		&I->flash_increment_events, pages_number_events, page_size, erase_page_events_callback,
		read_page_events_callback, write_page_events_callback));

	ASSERT_FLASH_INCREMENT_NOT_OK(flash_increment_init(
		&I->flash_increment_header, pages_number_header, page_size, erase_page_header_callback,
		read_page_header_callback, write_page_header_callback));

	ASSERT_FLASH_INCREMENT_NOT_OK(
		flash_increment_get_max_flash_size(&I->flash_increment_header, &hdr_sector_bytes));

	for(nb_of_bytes_read = 0; nb_of_bytes_read < hdr_sector_bytes; nb_of_bytes_read += sizeof(unsigned))
	{
		ASSERT_FLASH_INCREMENT_NOT_OK(
			flash_increment_set_read_offset(&I->flash_increment_header, nb_of_bytes_read));
		ASSERT_FLASH_INCREMENT_NOT_OK(
			flash_increment_read(&I->flash_increment_header, &val_read, sizeof(unsigned)));
		;

		if(memcmp(&val_read, &val_empty, sizeof(unsigned)) == 0)
		{
			break;
		}
		{
			next_write_offset = val_read;
		}
	}

	/* If empty flash encountered on the first bytes, then assuming that flash is totally empty */
	if(nb_of_bytes_read == 0)
	{
		I->max_saved_counter_value = 0;
		next_write_offset = 0;
	}
	else
	{
		/* Read last stored element header to get last element's counter value */
		ASSERT_FLASH_INCREMENT_NOT_OK(flash_increment_set_read_offset(
			&I->flash_increment_events, next_write_offset - sizeof(eventlog_element_hdr_t)));
		ASSERT_FLASH_INCREMENT_NOT_OK(
			flash_increment_read(&I->flash_increment_events, &last_hdr, sizeof(eventlog_element_hdr_t)));

		I->max_saved_counter_value = last_hdr.counter;
	}

	ASSERT_FLASH_INCREMENT_NOT_OK(
		flash_increment_set_write_offset(&I->flash_increment_events, next_write_offset));

	return EVENTLOG_STATUS_OK;
}

eventlog_status_t eventlog_write(eventlog_t* const I, const type_t type, time_t time, const uint8_t* payload,
								 size_t payload_size)
{
	uint8_t eventlog_buffer[sizeof(eventlog_element_extended_t)] = {0};
	uint16_t index = 0;
	crc_t calculated_crc = 0;
	eventlog_element_hdr_t element_header = {0};
	unsigned next_offset_to_write = 0;
	size_t payload_padding_size = 0;

	if(I == NULL)
	{
		return EVENTLOG_STATUS_ERROR;
	}

	if(payload_size > EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE)
	{
		payload_size = EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE;
	}

	I->max_saved_counter_value += 1;

	element_header.counter = I->max_saved_counter_value;
	element_header.type = type;
	if(time == 0)
	{
		time = 1UL;
	}
	element_header.timestamp = time;

	index += sizeof(crc_t);
	if(eventlog_is_event_extended(type))
	{
		if(payload != NULL)
		{
			memcpy(&eventlog_buffer[index], payload, payload_size);
		}
		else
		{
			memset(&eventlog_buffer[index], 0x00, payload_size);
		}
		index += payload_size;

		/* Considering size of eventlog_element_extended_t make sure that total event size will be
		 * a multiple of 8 bytes. */
		payload_padding_size = 7 - (payload_size % 8);
		if(payload_size + payload_padding_size > EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE)
		{
			payload_padding_size = EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE - payload_size;
		}
		memset(&eventlog_buffer[index], 0x00, payload_padding_size);
		index += payload_padding_size;
		eventlog_buffer[index] = payload_size + payload_padding_size;
		index += sizeof(uint8_t);
	}

	memcpy(&eventlog_buffer[index], &element_header, sizeof(eventlog_element_hdr_t));
	index += sizeof(eventlog_element_hdr_t);

	calculated_crc = eventlog_crc_calc(&eventlog_buffer[sizeof(crc_t)], index - sizeof(crc_t));
	memcpy(&eventlog_buffer[0], &calculated_crc, sizeof(crc_t));

	ASSERT_FLASH_INCREMENT_NOT_OK(
		flash_increment_write(&I->flash_increment_events, true, eventlog_buffer, index));

	ASSERT_FLASH_INCREMENT_NOT_OK(
		flash_increment_get_write_offset(&I->flash_increment_events, &next_offset_to_write));
	ASSERT_FLASH_INCREMENT_NOT_OK(flash_increment_write(&I->flash_increment_header, true,
														&next_offset_to_write, sizeof(next_offset_to_write)));

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

	unsigned actual_write_position = 0;
	uint8_t extended_element_buf[sizeof(eventlog_element_extended_t)] = {0};
	uint8_t curr_payload[EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE] = {0};
	uint32_t offset_cnt = 0;
	eventlog_element_hdr_t empty_hdr = {0};
	eventlog_element_hdr_t read_hdr = {0};
	uint8_t curr_payload_size = 0;
	crc_t curr_crc = 0;
	crc_t calculated_crc = 0;
	uint32_t last_correct_counter = 0xFFFFFFFF;

	uint32_t counter_le = 0;
	uint16_t type_le = 0;
	time_t time_le = 0;
	size_t reported_element_size = 0;

	memset(&empty_hdr, I->empty_flash_value, sizeof(eventlog_element_hdr_t));

	*received_size = 0;
	*number_of_reads = 0;

	ASSERT_FLASH_INCREMENT_NOT_OK(
		flash_increment_get_write_offset(&I->flash_increment_events, &actual_write_position));
	ASSERT_FLASH_INCREMENT_NOT_OK(
		flash_increment_set_read_offset(&I->flash_increment_events, actual_write_position));

	for(int x = 0; x < number_to_read + offset_of_read; ++x)
	{
		ASSERT_EVENTLOG_STATUS_NOT_OK(
			eventlog_read_back(&I->flash_increment_events, &read_hdr, sizeof(eventlog_element_hdr_t)));

		/* If encountered empty flash or unexpected values in the header */
		if(memcmp(&read_hdr, &empty_hdr, sizeof(eventlog_element_hdr_t)) == 0 ||
		   eventlog_is_junk_in_header(&read_hdr, last_correct_counter) && x > 0)
		{
			/* Reached the end of stored events, this will occur only if events did not wrap around in storage
			 */
			if(last_correct_counter <= 1)
			{
				return EVENTLOG_STATUS_OK;
			}
			else
			{
				/* Encountered empty flash, check if it is not a flash write error */
				for(uint8_t i = 0; i < EVENTLOG_MAX_RECOVERY_TRIES_NB; ++i)
				{
					ASSERT_EVENTLOG_STATUS_NOT_OK(eventlog_read_back(&I->flash_increment_events, &read_hdr,
																	 sizeof(eventlog_element_hdr_t)));
					/* Check if values in the just read header are as expected */
					/* CRC will be checked in the later part of loop, size is not known now */
					if(eventlog_is_junk_in_header(&read_hdr, last_correct_counter))
					{
						if(i == EVENTLOG_MAX_RECOVERY_TRIES_NB - 1)
						{
							/* Return OK, events that were read up till now are correct */
							return EVENTLOG_STATUS_OK;
						}
						else
						{
							/* Move back 8 bytes (events are 8 byte aligned), check again */
							ASSERT_FLASH_INCREMENT_NOT_OK(
								flash_increment_move_left_read_offset(&I->flash_increment_events, 8));
						}
					}
					else
					{
						break;
					}
				}
			}
		}

		ASSERT_FLASH_INCREMENT_NOT_OK(flash_increment_move_left_read_offset(&I->flash_increment_events,
																			sizeof(eventlog_element_hdr_t)));

		if(eventlog_is_event_extended(read_hdr.type))
		{
			memset(curr_payload, 0, EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE);
			memset(extended_element_buf, 0, EVENTLOG_EXTENDED_EVENT_MAX_PAYLOAD_SIZE);

			ASSERT_EVENTLOG_STATUS_NOT_OK(eventlog_read_back_and_move_left(
				&I->flash_increment_events, &curr_payload_size, sizeof(uint8_t)));

			ASSERT_EVENTLOG_STATUS_NOT_OK(eventlog_read_back_and_move_left(&I->flash_increment_events,
																		   &curr_payload, curr_payload_size));

			ASSERT_EVENTLOG_STATUS_NOT_OK(
				eventlog_read_back_and_move_left(&I->flash_increment_events, &curr_crc, sizeof(crc_t)));


			memcpy(extended_element_buf, curr_payload, curr_payload_size);
			memcpy(extended_element_buf + curr_payload_size, &curr_payload_size, sizeof(uint8_t));
			memcpy(extended_element_buf + curr_payload_size + sizeof(uint8_t), &read_hdr,
				   sizeof(eventlog_element_hdr_t));

			calculated_crc = eventlog_crc_calc(extended_element_buf, curr_payload_size + sizeof(uint8_t) +
																		 sizeof(eventlog_element_hdr_t));

			reported_element_size = sizeof(eventlog_element_hdr_t) + sizeof(uint8_t) + curr_payload_size;
		}
		else
		{
			ASSERT_EVENTLOG_STATUS_NOT_OK(
				eventlog_read_back_and_move_left(&I->flash_increment_events, &curr_crc, sizeof(crc_t)));

			calculated_crc = eventlog_crc_calc(&read_hdr, sizeof(eventlog_element_hdr_t));

			reported_element_size = sizeof(eventlog_element_t) - sizeof(crc_t);
			curr_payload_size = 0;
		}

		if(curr_crc != calculated_crc)
		{
			continue;
		}


#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		counter_le = end_be32toh(read_hdr.counter);
		type_le = end_be16toh(read_hdr.type);
		time_le = end_be64toh(read_hdr.timestamp);
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	#error Check behaviour of reading on big endian system, this was not verified.
#endif

		last_correct_counter = read_hdr.counter;

		if(offset_cnt < offset_of_read)
		{
			offset_cnt++;
		}
		else
		{
			if(*received_size + reported_element_size > max_buffer_size)
			{
				return EVENTLOG_STATUS_OK;
			}

			memcpy(buffer + *received_size, &counter_le, sizeof(counter_t));
			memcpy(buffer + *received_size + sizeof(counter_t), &type_le, sizeof(type_t));
			memcpy(buffer + *received_size + sizeof(counter_t) + sizeof(type_t), &time_le, sizeof(time_t));

			if(curr_payload_size > 0)
			{
				memcpy(buffer + *received_size + sizeof(counter_t) + sizeof(type_t) + sizeof(time_t),
					   &curr_payload_size, sizeof(uint8_t));
				memcpy(buffer + *received_size + sizeof(counter_t) + sizeof(type_t) + sizeof(time_t) +
						   sizeof(uint8_t),
					   &curr_payload, curr_payload_size);
			}

			*received_size += reported_element_size;
			*number_of_reads += 1;
		}
	}

	return EVENTLOG_STATUS_OK;
}

eventlog_status_t eventlog_erase(eventlog_t* const I)
{
	ASSERT_FLASH_INCREMENT_NOT_OK(flash_increment_clear_memory(&I->flash_increment_events));
	ASSERT_FLASH_INCREMENT_NOT_OK(flash_increment_clear_memory(&I->flash_increment_header));

	I->max_saved_counter_value = 0;

	return EVENTLOG_STATUS_OK;
}
