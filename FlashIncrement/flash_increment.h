#ifndef FLASH_INCREMENT_H
#define FLASH_INCREMENT_H


#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"


typedef enum flash_increment_status
{
	FLASH_INCREMENT_STATUS_ERROR,
	FLASH_INCREMENT_STATUS_LOGIC_ERROR,
	FLASH_INCREMENT_STATUS_OK,
} flash_increment_status_t;


typedef flash_increment_status_t (*flash_increment_erase_page_t)(unsigned int page);
typedef flash_increment_status_t (*flash_increment_read_page_t)(unsigned int page,
																const uint32_t address_offset,
																char* const buffer, const size_t size);
typedef flash_increment_status_t (*flash_increment_write_page_t)(unsigned int page,
																 const uint32_t address_offset,
																 const char* const buffer, const size_t size);


typedef struct flash_increment
{
		unsigned int pages;
		size_t page_size;

		unsigned int actual_write_managed_bytes;
		unsigned int actual_read_managed_bytes;

		flash_increment_erase_page_t erase_page_callback;
		flash_increment_read_page_t read_page_callback;
		flash_increment_write_page_t write_page_callback;

		// const
		unsigned int total_max_data;
} flash_increment_t;


flash_increment_status_t flash_increment_init(flash_increment_t* main, unsigned pages_number,
											  unsigned page_size,
											  flash_increment_erase_page_t erase_page_callback,
											  flash_increment_read_page_t read_page_callback,
											  flash_increment_write_page_t write_page_callback);

flash_increment_status_t flash_increment_set_write_offset(flash_increment_t* main, unsigned offset);
flash_increment_status_t flash_increment_move_left_write_offset(flash_increment_t* main, unsigned value);
flash_increment_status_t flash_increment_move_right_write_offset(flash_increment_t* main, unsigned value);
flash_increment_status_t flash_increment_get_write_offset(flash_increment_t* main, unsigned* offset);
flash_increment_status_t flash_increment_write(flash_increment_t* main, bool erase, const void* data,
											   size_t data_size);

flash_increment_status_t flash_increment_set_read_offset(flash_increment_t* main, unsigned offset);
flash_increment_status_t flash_increment_move_left_read_offset(flash_increment_t* main, unsigned value);
flash_increment_status_t flash_increment_move_right_read_offset(flash_increment_t* main, unsigned value);
flash_increment_status_t flash_increment_move_read_offset_to_next_page_begin(flash_increment_t* main);
flash_increment_status_t flash_increment_get_read_offset(flash_increment_t* main, unsigned* offset);
flash_increment_status_t flash_increment_read(flash_increment_t* main, void* data, size_t data_size);

flash_increment_status_t flash_increment_clear_memory(flash_increment_t* main);
flash_increment_status_t flash_increment_get_pages(flash_increment_t* main, unsigned* pages);
flash_increment_status_t flash_increment_get_page_size(flash_increment_t* main, size_t* page_size);
flash_increment_status_t flash_increment_get_max_flash_size(flash_increment_t* main, unsigned* bytes);

flash_increment_status_t flash_increment_is_read_and_write_indexes_on_the_same_page(flash_increment_t* main,
																					bool* value);
flash_increment_status_t flash_increment_is_read_and_write_indexes_the_same(flash_increment_t* main,
																			bool* value);
flash_increment_status_t flash_increment_is_write_index_bigger_than_read(flash_increment_t* main,
																		 bool* value);


#endif // FLASH_INCREMENT_H
