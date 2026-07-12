#include "flash_increment.h"


static flash_increment_status_t read_offset(flash_increment_t* const main, const unsigned offset,
											void* const data, const size_t data_size)
{
	// Check input pointers and size before calculating flash indexes.
	if(main == NULL || data == NULL || data_size == 0)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	// One read operation cannot be bigger than whole managed memory.
	if(data_size > main->total_max_data)
	{
		return FLASH_INCREMENT_STATUS_LOGIC_ERROR;
	}


	flash_increment_status_t status;

	// Normalize read offset to managed flash area.
	unsigned int offset_local = offset % main->total_max_data;
	// Calculate page number for actual read offset.
	unsigned int actual_page_number = offset_local / main->page_size;
	// Calculate absolute byte index of end of actual page.
	unsigned int max_data_bytes_for_all_pages_to_actual = (actual_page_number + 1) * main->page_size;
	// Calculate byte offset inside actual page.
	unsigned int actual_page_buffer_read_position = offset_local - (actual_page_number * main->page_size);


	// Read data directly when it does not exceed actual page.
	if(offset_local + data_size <= max_data_bytes_for_all_pages_to_actual)
	{
		status =
			main->read_page_callback(actual_page_number, actual_page_buffer_read_position, data, data_size);
		if(status != FLASH_INCREMENT_STATUS_OK)
		{
			return FLASH_INCREMENT_STATUS_ERROR;
		}
	}
	// Read some bytes on last used page, rest from next page.
	else
	{
		// Calculate how many bytes can be read from actual page.
		unsigned int first_half_read_bytes_number = main->page_size - actual_page_buffer_read_position;
		// Calculate how many bytes must be read from next page/pages.
		unsigned int second_half_read_bytes_number = data_size - first_half_read_bytes_number;


		// Read first part from actual page.
		status = main->read_page_callback(actual_page_number, actual_page_buffer_read_position, data,
										  first_half_read_bytes_number);
		if(status != FLASH_INCREMENT_STATUS_OK)
		{
			return FLASH_INCREMENT_STATUS_ERROR;
		}

		// Move local read offset after already read bytes.
		offset_local += first_half_read_bytes_number;

		// Move read byte value at the beginning of flash.
		if(offset_local >= main->total_max_data)
		{
			offset_local = 0;
		}

		// Read remaining data from calculated next offset.
		status = read_offset(main, offset_local, (char* const) data + first_half_read_bytes_number,
							 second_half_read_bytes_number);

		if(status != FLASH_INCREMENT_STATUS_OK)
		{
			return FLASH_INCREMENT_STATUS_ERROR;
		}
	}

	return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t write_actual_page(flash_increment_t* const main, const bool erase,
												  const void* const data, const size_t data_size)
{
	flash_increment_status_t status;

	// Calculate page number for actual write offset.
	unsigned int actual_page_number = main->actual_write_managed_bytes / main->page_size;

	// Erase page only when write offset is at the beginning of the page.
	if(main->actual_write_managed_bytes % main->page_size == 0 && erase == true)
	{
		status = main->erase_page_callback(actual_page_number);
		if(status != FLASH_INCREMENT_STATUS_OK)
		{
			return FLASH_INCREMENT_STATUS_ERROR;
		}
	}

	// Calculate byte offset inside actual page.
	unsigned int actual_page_buffer_write_position =
		main->actual_write_managed_bytes - (actual_page_number * main->page_size);

	// Write only bytes which fit into actual page.
	status =
		main->write_page_callback(actual_page_number, actual_page_buffer_write_position, data, data_size);
	if(status != FLASH_INCREMENT_STATUS_OK)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	// Move global write offset after saved bytes.
	main->actual_write_managed_bytes += data_size;

	return FLASH_INCREMENT_STATUS_OK;
}


flash_increment_status_t flash_increment_init(flash_increment_t* const main, const unsigned pages_number,
											  const unsigned page_size,
											  flash_increment_erase_page_t erase_page_callback,
											  flash_increment_read_page_t read_page_callback,
											  flash_increment_write_page_t write_page_callback)
{
	if(pages_number == 0 || page_size == 0)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	if(main == NULL || erase_page_callback == NULL || read_page_callback == NULL ||
	   write_page_callback == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	main->pages = pages_number;
	main->page_size = page_size;

	main->actual_write_managed_bytes = 0;
	main->actual_read_managed_bytes = 0;

	main->erase_page_callback = erase_page_callback;
	main->read_page_callback = read_page_callback;
	main->write_page_callback = write_page_callback;

	main->total_max_data = pages_number * page_size;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_set_write_offset(flash_increment_t* main, const unsigned offset)
{
	if(main == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	main->actual_write_managed_bytes = offset % main->total_max_data;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_move_left_write_offset(flash_increment_t* main, unsigned value)
{
	if(main == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	value %= main->total_max_data;

	if(main->actual_write_managed_bytes >= value)
	{
		main->actual_write_managed_bytes -= value;
	}
	else
	{
		main->actual_write_managed_bytes = main->total_max_data - (value - main->actual_write_managed_bytes);
	}

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_move_right_write_offset(flash_increment_t* main, unsigned value)
{
	if(main == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	main->actual_write_managed_bytes += value;
	main->actual_write_managed_bytes %= main->total_max_data;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_get_write_offset(flash_increment_t* const main,
														  unsigned* const offset)
{
	if(main == NULL || offset == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	*offset = main->actual_write_managed_bytes;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_write(flash_increment_t* const main, const bool erase,
											   const void* const data, const size_t data_size)
{
	// Check input pointer and write size before calculating flash indexes.
	if(main == NULL || data_size == 0)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	// One write operation cannot be bigger than whole managed memory.
	if(data_size > main->total_max_data)
	{
		return FLASH_INCREMENT_STATUS_LOGIC_ERROR;
	}

	flash_increment_status_t status;
	size_t data_size_to_save = data_size;
	size_t saved_data_size = 0;

	while(data_size_to_save > 0)
	{
		// Move write byte value at the beginning of flash.
		if(main->actual_write_managed_bytes >= main->total_max_data)
		{
			main->actual_write_managed_bytes = 0;
		}

		// Calculate byte offset inside actual page.
		unsigned int actual_page_buffer_write_position = main->actual_write_managed_bytes % main->page_size;
		// Calculate how many bytes can be written into actual page.
		unsigned int actual_page_free_bytes_number = main->page_size - actual_page_buffer_write_position;

		// Limit actual write to page end or to remaining data size.
		size_t actual_data_size_to_save = data_size_to_save;
		if(actual_data_size_to_save > actual_page_free_bytes_number)
		{
			actual_data_size_to_save = actual_page_free_bytes_number;
		}

		// Write calculated part of data into actual page.
		status = write_actual_page(main, erase, (const char* const) data + saved_data_size,
								   actual_data_size_to_save);
		if(status != FLASH_INCREMENT_STATUS_OK)
		{
			return FLASH_INCREMENT_STATUS_ERROR;
		}

		// Move source data pointer and decrease number of bytes left to write.
		saved_data_size += actual_data_size_to_save;
		data_size_to_save -= actual_data_size_to_save;
	}

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_set_read_offset(flash_increment_t* main, unsigned offset)
{
	if(main == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	main->actual_read_managed_bytes = offset % main->total_max_data;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_move_left_read_offset(flash_increment_t* main, unsigned value)
{
	if(main == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	value %= main->total_max_data;

	if(main->actual_read_managed_bytes >= value)
	{
		main->actual_read_managed_bytes -= value;
	}
	else
	{
		main->actual_read_managed_bytes = main->total_max_data - (value - main->actual_read_managed_bytes);
	}

	return FLASH_INCREMENT_STATUS_OK;
}
flash_increment_status_t flash_increment_move_right_read_offset(flash_increment_t* main, unsigned value)
{
	if(main == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	main->actual_read_managed_bytes += value;
	main->actual_read_managed_bytes %= main->total_max_data;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_move_read_offset_to_next_page_begin(flash_increment_t* const main)
{
	if(main == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	unsigned int actual_read_page_number = main->actual_read_managed_bytes / main->page_size;

	actual_read_page_number += 1;
	actual_read_page_number %= main->pages;

	main->actual_read_managed_bytes = actual_read_page_number * main->page_size;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_get_read_offset(flash_increment_t* main, unsigned* offset)
{
	if(main == NULL || offset == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	*offset = main->actual_read_managed_bytes;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_read(flash_increment_t* const main, void* const data,
											  const size_t data_size)
{
	flash_increment_status_t status_flash_increment;

	// Read from actual read offset without changing it before operation is finished.
	status_flash_increment = read_offset(main, main->actual_read_managed_bytes, data, data_size);

	// Move read offset only after successful read operation.
	if(status_flash_increment == FLASH_INCREMENT_STATUS_OK)
	{
		// Move read offset by read bytes and wrap it to flash beginning when needed.
		main->actual_read_managed_bytes += data_size;
		main->actual_read_managed_bytes %= main->total_max_data;
	}

	return status_flash_increment;
}

flash_increment_status_t flash_increment_clear_memory(flash_increment_t* const main)
{
	flash_increment_status_t status;

	for(unsigned x = 0; x < main->pages; ++x)
	{
		status = main->erase_page_callback(x);
		if(status != FLASH_INCREMENT_STATUS_OK)
		{
			return FLASH_INCREMENT_STATUS_ERROR;
		}
	}

	main->actual_write_managed_bytes = 0;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_get_pages(flash_increment_t* main, unsigned* pages)
{
	if(main == NULL || pages == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	*pages = main->pages;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_get_page_size(flash_increment_t* main, size_t* page_size)
{
	if(main == NULL || page_size == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	*page_size = main->page_size;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_get_max_flash_size(flash_increment_t* main, unsigned* bytes)
{
	if(main == NULL || bytes == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	*bytes = main->pages * main->page_size;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t
flash_increment_is_read_and_write_indexes_on_the_same_page(flash_increment_t* const main, bool* const value)
{
	if(main == NULL || value == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	unsigned int actual_write_page_number = main->actual_write_managed_bytes / main->page_size;
	unsigned int actual_read_page_number = main->actual_read_managed_bytes / main->page_size;

	*value = actual_write_page_number == actual_read_page_number;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_is_read_and_write_indexes_the_same(flash_increment_t* const main,
																			bool* const value)
{
	if(main == NULL || value == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	*value = main->actual_write_managed_bytes == main->actual_read_managed_bytes;

	return FLASH_INCREMENT_STATUS_OK;
}

flash_increment_status_t flash_increment_is_write_index_bigger_than_read(flash_increment_t* const main,
																		 bool* const value)
{
	if(main == NULL || value == NULL)
	{
		return FLASH_INCREMENT_STATUS_ERROR;
	}

	*value = main->actual_write_managed_bytes > main->actual_read_managed_bytes;

	return FLASH_INCREMENT_STATUS_OK;
}
