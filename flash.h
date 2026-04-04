#ifndef FLASH_H_
#define FLASH_H_

#include "nrfx_nvmc.h"

#include <stdint.h>
#include <stdbool.h>


bool flash_write(uint32_t address, const void *data, size_t count_bytes);
bool flash_read(uint32_t address, void *data, size_t count_bytes);

#endif // FLASH_H_