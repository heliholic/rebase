/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#include "build/build_config.h"

#include "common/crc.h"
#include "common/utils.h"

#include "config/config_eeprom.h"
#include "config/config_eeprom_impl.h"
#include "config/config_streamer.h"
#include "config/config_streamer_impl.h"

#include "pg/pg.h"
#include "config/config.h"

#ifdef CONFIG_IN_SDCARD
#include "io/asyncfatfs/asyncfatfs.h"
#endif

#include "drivers/flash/flash.h"
#include "drivers/system.h"

static uint16_t eepromConfigSize;

#define CRC_START_VALUE         0xFFFF
#define CRC_CHECK_VALUE         0x1D0F  // pre-calculated value of CRC that includes the CRC itself

// Header for the saved copy.
typedef struct {
    uint32_t magic;           // magic number, should be EEPROM_CONFIG_MAGIC
    uint32_t version;
} PG_PACKED configHeader_t;

// Header for each stored PG.
typedef struct {
    uint32_t hash;
    uint16_t size;
    uint8_t pg[];
} PG_PACKED configRecord_t;

// Footer for the saved copy.
typedef struct {
    uint32_t terminator;
} PG_PACKED configFooter_t;
// checksum is appended just after footer. It is not included in footer to make checksum calculation consistent

// Used to check the compiler packing at build time.
typedef struct {
    uint8_t byte;
    uint32_t word;
} PG_PACKED packingTest_t;

#if defined(CONFIG_IN_EXTERNAL_FLASH) || defined(CONFIG_IN_MEMORY_MAPPED_FLASH)
static MMFLASH_CODE bool loadEEPROMFromExternalFlash(void)
{
    const flashPartition_t *flashPartition = flashPartitionFindByType(FLASH_PARTITION_TYPE_CONFIG);
    const flashGeometry_t *flashGeometry = flashGetGeometry();
    uint32_t flashStartAddress = flashPartition->startSector * flashGeometry->sectorSize;

    uint32_t totalBytesRead = 0;
    int bytesRead = 0;

    bool success = false;
#ifdef CONFIG_IN_MEMORY_MAPPED_FLASH
    flashMemoryMappedModeDisable();
#endif
    do {
        bytesRead = flashReadBytes(flashStartAddress + totalBytesRead, &eepromData[totalBytesRead], EEPROM_SIZE - totalBytesRead);
        if (bytesRead > 0) {
            totalBytesRead += bytesRead;
            success = (totalBytesRead == EEPROM_SIZE);
        }
    } while (!success && bytesRead > 0);
#ifdef CONFIG_IN_MEMORY_MAPPED_FLASH
    flashMemoryMappedModeEnable();
#endif

    return success;
}

#ifdef CONFIG_IN_MEMORY_MAPPED_FLASH
MMFLASH_CODE_NOINLINE void saveEEPROMToMemoryMappedFlash(void)
{
    const flashPartition_t *flashPartition = flashPartitionFindByType(FLASH_PARTITION_TYPE_CONFIG);
    const flashGeometry_t *flashGeometry = flashGetGeometry();

    uint32_t flashSectorSize = flashGeometry->sectorSize;
    uint32_t flashPageSize = flashGeometry->pageSize;
    uint32_t flashStartAddress = flashPartition->startSector * flashGeometry->sectorSize;

    uint32_t bytesRemaining = EEPROM_SIZE;
    uint32_t offset = 0;

    flashMemoryMappedModeDisable();

    do {
        uint32_t flashAddress = flashStartAddress + offset;

        uint32_t bytesToWrite = bytesRemaining;
        if (bytesToWrite > flashPageSize) {
            bytesToWrite = flashPageSize;
        }

        bool onSectorBoundary = flashAddress % flashSectorSize == 0;
        if (onSectorBoundary) {
            flashEraseSector(flashAddress);
        }

        flashPageProgram(flashAddress, (uint8_t *)&eepromData[offset], bytesToWrite, NULL);

        bytesRemaining -= bytesToWrite;
        offset += bytesToWrite;
    } while (bytesRemaining > 0);

    flashWaitForReady();

    flashMemoryMappedModeEnable();
}
#endif

#elif defined(CONFIG_IN_SDCARD)

enum {
    FILE_STATE_NONE = 0,
    FILE_STATE_BUSY = 1,
    FILE_STATE_FAILED,
    FILE_STATE_COMPLETE,
};

uint8_t fileState = FILE_STATE_NONE;

const char *defaultSDCardConfigFilename = "CONFIG.BIN";

void saveEEPROMToSDCardCloseContinue(void)
{
    if (fileState != FILE_STATE_FAILED) {
        fileState = FILE_STATE_COMPLETE;
    }
}

void saveEEPROMToSDCardWriteContinue(afatfsFilePtr_t file)
{
    if (!file) {
        fileState = FILE_STATE_FAILED;
        return;
    }

    uint32_t totalBytesWritten = 0;
    uint32_t bytesWritten = 0;
    bool success;

    do {
        bytesWritten = afatfs_fwrite(file, &eepromData[totalBytesWritten], EEPROM_SIZE - totalBytesWritten);
        totalBytesWritten += bytesWritten;
        success = (totalBytesWritten == EEPROM_SIZE);

        afatfs_poll();
    } while (!success && afatfs_getLastError() == AFATFS_ERROR_NONE);

    if (!success) {
        fileState = FILE_STATE_FAILED;
    }

    while (!afatfs_fclose(file, saveEEPROMToSDCardCloseContinue)) {
        afatfs_poll();
    }
}

bool saveEEPROMToSDCard(void)
{
    fileState = FILE_STATE_BUSY;
    bool result = afatfs_fopen(defaultSDCardConfigFilename, "w+", saveEEPROMToSDCardWriteContinue);
    if (!result) {
        return false;
    }

    while (fileState == FILE_STATE_BUSY) {
        afatfs_poll();
    }

    while (!afatfs_flush()) {
        afatfs_poll();
    };

    return (fileState == FILE_STATE_COMPLETE);
}

void loadEEPROMFromSDCardCloseContinue(void)
{
    if (fileState != FILE_STATE_FAILED) {
        fileState = FILE_STATE_COMPLETE;
    }
}

void loadEEPROMFromSDCardReadContinue(afatfsFilePtr_t file)
{
    if (!file) {
        fileState = FILE_STATE_FAILED;
        return;
    }

    fileState = FILE_STATE_BUSY;

    uint32_t totalBytesRead = 0;
    uint32_t bytesRead = 0;
    bool success;

    if (afatfs_feof(file)) {
        // empty file, nothing to load.
        memset(eepromData, 0x00, EEPROM_SIZE);
        success = true;
    } else {

        do {
            bytesRead = afatfs_fread(file, &eepromData[totalBytesRead], EEPROM_SIZE - totalBytesRead);
            totalBytesRead += bytesRead;
            success = (totalBytesRead == EEPROM_SIZE);

            afatfs_poll();
        } while (!success && afatfs_getLastError() == AFATFS_ERROR_NONE);
    }

    if (!success) {
        fileState = FILE_STATE_FAILED;
    }

    while (!afatfs_fclose(file, loadEEPROMFromSDCardCloseContinue)) {
        afatfs_poll();
    }

    return;
}

bool loadEEPROMFromSDCard(void)
{
    fileState = FILE_STATE_BUSY;
    // use "w+" mode here to ensure the file is created now - in w+ mode we can read and write and the seek position is 0 on existing files, ready for reading.
    bool result = afatfs_fopen(defaultSDCardConfigFilename, "w+", loadEEPROMFromSDCardReadContinue);
    if (!result) {
        return false;
    }

    while (fileState == FILE_STATE_BUSY) {
        afatfs_poll();
    }

    return (fileState == FILE_STATE_COMPLETE);
}
#endif

void initEEPROM(void)
{
    // Verify that this architecture packs as expected.
    STATIC_ASSERT(offsetof(packingTest_t, byte) == 0, byte_packing_test_failed);
    STATIC_ASSERT(offsetof(packingTest_t, word) == 1, word_packing_test_failed);
    STATIC_ASSERT(sizeof(packingTest_t) == 5, overall_packing_test_failed);

    STATIC_ASSERT(sizeof(configFooter_t) == 4, footer_size_failed);
    STATIC_ASSERT(sizeof(configRecord_t) == 6, record_size_failed);

#if defined(CONFIG_IN_FILE)
    bool eepromLoaded = loadEEPROMFromFile();
    if (!eepromLoaded) {
        // File read failed - just die now
        failureMode(FAILURE_FILE_READ_FAILED);
    }
#elif defined(CONFIG_IN_EXTERNAL_FLASH) || defined(CONFIG_IN_MEMORY_MAPPED_FLASH)
    bool eepromLoaded = loadEEPROMFromExternalFlash();
    if (!eepromLoaded) {
        // Flash read failed - just die now
        failureMode(FAILURE_FLASH_READ_FAILED);
    }
#elif defined(CONFIG_IN_SDCARD)
    bool eepromLoaded = loadEEPROMFromSDCard();
    if (!eepromLoaded) {
        // SDCard read failed - just die now
        failureMode(FAILURE_SDCARD_READ_FAILED);
    }
#endif
}

bool isEEPROMVersionValid(void)
{
    const uint8_t *ptr = (const uint8_t*)&__config_start;
    const configHeader_t *header = (const configHeader_t *)ptr;

    return (header->version == EEPROM_CONFIG_VERSION);
}

// Scan the EEPROM config. Returns true if the config is valid.
bool isEEPROMStructureValid(void)
{
    const uint8_t *ptr = (const uint8_t*)&__config_start;
    const configHeader_t *header = (const configHeader_t *)ptr;

    if (header->magic != EEPROM_CONFIG_MAGIC)
        return false;

    uint16_t crc = CRC_START_VALUE;
    crc = crc16_ccitt_update(crc, header, sizeof(*header));
    ptr += sizeof(*header);

    for (;;) {
        const configRecord_t *record = (const configRecord_t *)ptr;

        // Found the end.  Stop scanning.
        if (record->hash == 0) {
            break;
        }

        // Too big or too small.
        if (ptr + record->size >= (const uint8_t*)&__config_end ||
            record->size < sizeof(*record)) {
            return false;
        }

        crc = crc16_ccitt_update(crc, ptr, record->size);
        ptr += record->size;
    }

    const configFooter_t *footer = (const configFooter_t *)ptr;
    crc = crc16_ccitt_update(crc, footer, sizeof(*footer));
    ptr += sizeof(*footer);

    // include stored CRC in the CRC calculation
    const uint16_t *storedCrc = (const uint16_t *)ptr;
    crc = crc16_ccitt_update(crc, storedCrc, sizeof(*storedCrc));
    ptr += sizeof(storedCrc);

    eepromConfigSize = ptr - (const uint8_t*)&__config_start;

    // CRC has the property that if the CRC itself is included in the calculation the resulting CRC will have constant value
    return crc == CRC_CHECK_VALUE;
}

uint16_t getEEPROMConfigSize(void)
{
    return eepromConfigSize;
}

size_t getEEPROMStorageSize(void)
{
#if defined(CONFIG_IN_EXTERNAL_FLASH) || defined(CONFIG_IN_MEMORY_MAPPED_FLASH)

    const flashPartition_t *flashPartition = flashPartitionFindByType(FLASH_PARTITION_TYPE_CONFIG);
    return FLASH_PARTITION_SECTOR_COUNT(flashPartition) * flashGetGeometry()->sectorSize;
#endif
#ifdef CONFIG_IN_RAM
    return EEPROM_SIZE;
#else
    return (const uint8_t*)&__config_end - (const uint8_t*)&__config_start;
#endif
}

// find config record for reg in EEPROM
// return NULL when record is not found
// this function assumes that EEPROM content is valid
static const configRecord_t *findEEPROM(const pgRegistry_t *reg)
{
    const uint8_t *ptr = (const uint8_t*)&__config_start;
    ptr += sizeof(configHeader_t);

    while (true) {
        const configRecord_t *record = (const configRecord_t *)ptr;
        if (record->size == 0 ||
            ptr + record->size >= (const uint8_t*)&__config_end ||
            record->size < sizeof(*record))
            break;
        if (pgHash(reg) == record->hash)
            return record;
        ptr += record->size;
    }

    return NULL;
}

// Initialize all PG records from EEPROM.
// This functions processes all PGs sequentially, scanning EEPROM for each one. This is suboptimal,
//   but each PG is loaded/initialized exactly once and in defined order.
bool loadEEPROM(void)
{
    bool success = true;

    PG_FOREACH(reg) {
        const configRecord_t *rec = findEEPROM(reg);
        if (rec) {
            // config from EEPROM is available, use it to initialize PG. pgLoad will handle version mismatch
            if (!pgLoad(reg, rec->pg, rec->size - offsetof(configRecord_t, pg), rec->hash)) {
                success = false;
            }
        } else {
            pgReset(reg);
            success = false;
        }
        *pgChecksum(reg) = fnv_update(FNV_OFFSET_BASIS, pgAddress(reg), pgSize(reg));
    }

    return success;
}

static bool writeSettingsToEEPROM(void)
{
    bool dirtyConfig = !isEEPROMVersionValid() || !isEEPROMStructureValid();

    configHeader_t header = {
        .magic = EEPROM_CONFIG_MAGIC,
        .version = EEPROM_CONFIG_VERSION,
    };

    PG_FOREACH(reg) {
        if (*pgChecksum(reg) != fnv_update(FNV_OFFSET_BASIS, pgAddress(reg), pgSize(reg))) {
            dirtyConfig = true;
        }
    }

    // Only write the config if it has changed
    if (dirtyConfig) {
        config_streamer_t streamer;
        config_streamer_init(&streamer);
        config_streamer_start(&streamer, (uintptr_t)&__config_start, (const uint8_t*)&__config_end - (const uint8_t*)&__config_start);
        config_streamer_write(&streamer, (uint8_t *)&header, sizeof(header));

        uint16_t crc = CRC_START_VALUE;
        crc = crc16_ccitt_update(crc, (uint8_t *)&header, sizeof(header));

        PG_FOREACH(reg) {
            configRecord_t record = {
                .hash = pgHash(reg),
                .size = sizeof(configRecord_t) + pgSize(reg),
            };

            config_streamer_write(&streamer, (uint8_t *)&record, sizeof(record));
            crc = crc16_ccitt_update(crc, (uint8_t *)&record, sizeof(record));
            config_streamer_write(&streamer, pgAddress(reg), pgSize(reg));
            crc = crc16_ccitt_update(crc, pgAddress(reg), pgSize(reg));
        }

        configFooter_t footer = {
            .terminator = 0,
        };

        config_streamer_write(&streamer, (uint8_t *)&footer, sizeof(footer));
        crc = crc16_ccitt_update(crc, (uint8_t *)&footer, sizeof(footer));

        // include inverted CRC in big endian format in the CRC
        const uint16_t invertedBigEndianCrc = ~(((crc & 0xFF) << 8) | (crc >> 8));
        config_streamer_write(&streamer, (uint8_t *)&invertedBigEndianCrc, sizeof(crc));
        config_streamer_flush(&streamer);

        return (config_streamer_finish(&streamer) == 0);
    }

    return true;
}

void writeConfigToEEPROM(void)
{
    bool success = false;
    // write it
    for (int attempt = 0; attempt < 3 && !success; attempt++) {
        if (writeSettingsToEEPROM() && isEEPROMVersionValid() && isEEPROMStructureValid()) {
            success = true;

#if defined(CONFIG_IN_EXTERNAL_FLASH) || defined(CONFIG_IN_MEMORY_MAPPED_FLASH)
            // copy it back from flash to the in-memory buffer.
            success = loadEEPROMFromExternalFlash();
#endif
#ifdef CONFIG_IN_SDCARD
            // copy it back from flash to the in-memory buffer.
            success = loadEEPROMFromSDCard();
#endif
        }
    }

    if (!success) {
        // Flash write failed - just die now
        failureMode(FAILURE_CONFIG_STORE_FAILURE);
    }
}
