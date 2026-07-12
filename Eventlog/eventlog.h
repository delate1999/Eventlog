#ifndef EVENTLOG_H
#define EVENTLOG_H


#include "stdio.h"
#include "stdlib.h"
#include "time.h"

#include "flash_increment.h"


typedef uint32_t counter_t;
typedef uint16_t type_t;
typedef uint16_t crc_t;


typedef enum
{
	EVENTLOG_STATUS_OK,
	EVENTLOG_STATUS_ERROR,
	EVENTLOG_STATUS_LOGIC_ERROR,
} eventlog_status_t;


typedef struct
{
		flash_increment_t flash_increment_events;
		flash_increment_t flash_increment_header;

		counter_t max_saved_counter_value;

		uint8_t empty_flash_value;
} eventlog_t;


eventlog_status_t eventlog_init(eventlog_t* const I, unsigned pages_number, unsigned page_size,
								uint8_t empty_flash_value,
								flash_increment_erase_page_t erase_page_events_callback,
								flash_increment_read_page_t read_page_events_callback,
								flash_increment_write_page_t write_page_events_callback,
								flash_increment_erase_page_t erase_page_header_callback,
								flash_increment_read_page_t read_page_header_callback,
								flash_increment_write_page_t write_page_header_callback);

eventlog_status_t eventlog_write(eventlog_t* I, type_t type, time_t time);
eventlog_status_t eventlog_read(eventlog_t* I, void* buffer, size_t* received_size, size_t max_buffer_size,
								size_t number_to_read, size_t* number_of_reads, size_t offset_of_read);
eventlog_status_t eventlog_erase(eventlog_t* I);


#endif // EVENTLOG_H
