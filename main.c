/** 
 * @brief Example usage of eventlog module.
 */

#include <stdio.h>
#include "eventlog.h"
#include "string.h"


#define PAGES_NUMBER 10
#define PAGE_SIZE 256
#define EMPTY_FLASH_VALUE 0xFF

static eventlog_t ctx = {0};

/* @note */
static uint8_t memory_for_events[PAGE_SIZE * PAGES_NUMBER] = {EMPTY_FLASH_VALUE};
static uint8_t memory_for_header[PAGE_SIZE * 1] = {EMPTY_FLASH_VALUE};

/* @note Due to the way the module flash increment is constructed seperate callbacks for writing to events and header pages are necessary. This will change.*/
/* @note Total number of pages - that is for data and header must be passed to init funciton. This will change. */

static flash_increment_status_t erase_page_events_callback(unsigned int page)
{
    if (page >= PAGES_NUMBER) {
        return FLASH_INCREMENT_STATUS_ERROR;
    }
    memset(&memory_for_events[page * PAGE_SIZE], EMPTY_FLASH_VALUE, PAGE_SIZE);
    return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t read_page_events_callback(unsigned int page, const uint32_t address_offset, char* const buffer, const size_t size)
{
    if (page >= PAGES_NUMBER || address_offset + size > PAGE_SIZE) {
        return FLASH_INCREMENT_STATUS_ERROR;
    }
    memcpy(buffer, &memory_for_events[page * PAGE_SIZE + address_offset], size);
    return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t write_page_events_callback(unsigned int page, const uint32_t address_offset, const char* const buffer, const size_t size)
{
    if (page >= PAGES_NUMBER || address_offset + size > PAGE_SIZE) {
        return FLASH_INCREMENT_STATUS_ERROR;
    }
    memcpy(&memory_for_events[page * PAGE_SIZE + address_offset], buffer, size);
    return FLASH_INCREMENT_STATUS_OK;
}


static flash_increment_status_t erase_page_header_callback(unsigned int page)
{
    if (page >= 1) {
        return FLASH_INCREMENT_STATUS_ERROR;
    }
    memset(&memory_for_header[page * PAGE_SIZE], EMPTY_FLASH_VALUE, PAGE_SIZE);
    return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t read_page_header_callback(unsigned int page, const uint32_t address_offset, char* const buffer, const size_t size)
{
    if (page >= 1 || address_offset + size > PAGE_SIZE) {
        return FLASH_INCREMENT_STATUS_ERROR;
    }
    memcpy(buffer, &memory_for_header[page * PAGE_SIZE + address_offset], size);
    return FLASH_INCREMENT_STATUS_OK;
}

static flash_increment_status_t write_page_header_callback(unsigned int page, const uint32_t address_offset, const char* const buffer, const size_t size)
{
    if (page >= 1 || address_offset + size > PAGE_SIZE) {
        return FLASH_INCREMENT_STATUS_ERROR;
    }
    memcpy(&memory_for_header[page * PAGE_SIZE + address_offset], buffer, size);
    return FLASH_INCREMENT_STATUS_OK;
}
     
void main(void){
    /* +1 page for header */
    eventlog_init(&ctx, (PAGES_NUMBER + 1), PAGE_SIZE, EMPTY_FLASH_VALUE, erase_page_events_callback, read_page_events_callback, 
                    write_page_events_callback, erase_page_header_callback, read_page_header_callback, write_page_header_callback);    
    printf("Eventlog initialized with %d pages of size %d bytes each.\n", PAGES_NUMBER, PAGE_SIZE);

    eventlog_write(&ctx, 1, time(NULL));
    eventlog_write(&ctx, 2, time(NULL));

    uint8_t buffer[128] = {0};
    size_t received_size = 0;
    size_t number_of_reads = 0;
    eventlog_read(&ctx, buffer, &received_size, sizeof(buffer), 2, &number_of_reads, 0);
    printf("Read %d bytes from eventlog. Read %d elements.\n", received_size, number_of_reads);
}

