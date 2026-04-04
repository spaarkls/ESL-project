#include "flash.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "defines.h"


bool flash_write(uint32_t address, const void *data, size_t count_bytes) {
    if (data == NULL) {
        return false;
    }

    if (address < FLASH_CONSTRAINTED_MIN_ADDRESS) {
        return false;
    }

    if (count_bytes > (FLASH_CONSTRAINTED_MAX_ADDRESS - address)) {
        return false;
    }


    nrfx_nvmc_page_erase(address);
    nrfx_nvmc_bytes_write(address, data, count_bytes);
    uint32_t time_out = 1000000;

    while (nrfx_nvmc_write_done_check() == false) {
        if (--time_out == 0) {
            return false;
        }
    }

    return true;
}


bool flash_read(uint32_t address, void *data, size_t count_bytes) {
    if (data == NULL) {
        return false;
    }

    if (address < FLASH_CONSTRAINTED_MIN_ADDRESS) {
        return false;
    }

    if (count_bytes > (FLASH_CONSTRAINTED_MAX_ADDRESS - address)) {
        return false;
    }

    memcpy(data, (void *)address, count_bytes);
    return true;
}

