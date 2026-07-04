#pragma once
#include "daisy_seed.h"
#include "per/qspi.h"
#include "sys/dma.h"

namespace daisy
{

template <typename SettingStruct, size_t NumSectors = 4>
class WearLevelingStorage
{
  public:
    /**
     * @brief Constructor for the wear-leveling storage class.
     * @param qspi Reference to the hardware QSPI peripheral.
     */
    WearLevelingStorage(QSPIHandle &qspi)
    : qspi_(qspi),
      address_offset_(0),
      default_settings_(),
      settings_(),
      latest_sector_(0),
      latest_slot_(0),
      max_seq_num_(0),
      has_valid_record_(false)
    {
    }

    /**
     * @brief Initialize storage and load the latest configuration.
     * 
     * Scans all sectors to find the record with the highest sequence number.
     * If no valid record is found, writes the default settings.
     * 
     * @param defaults Default configuration values.
     * @param address_offset Base offset on the QSPI chip (must be 4KB sector aligned).
     */
    void Init(const SettingStruct &defaults, uint32_t address_offset = 0)
    {
        default_settings_ = defaults;
        settings_         = defaults;
        address_offset_   = address_offset & ~static_cast<uint32_t>(0xFFF); // Align to 4KB sector boundary

        uint32_t max_seq = 0;
        bool found_any = false;

        for(size_t s = 0; s < NumSectors; ++s)
        {
            for(size_t slot = 0; slot < SLOTS_PER_SECTOR; ++slot)
            {
                uint32_t slot_offset = s * SECTOR_SIZE + slot * sizeof(WearLevelRecord);
                
                auto* record_ptr = reinterpret_cast<WearLevelRecord*>(
                    reinterpret_cast<uint8_t*>(qspi_.GetData(address_offset_)) + slot_offset
                );

#if !UNIT_TEST
                if(System::GetProgramMemoryRegion() != System::MemoryRegion::INTERNAL_FLASH)
                {
                    dsy_dma_invalidate_cache_for_buffer(reinterpret_cast<uint8_t*>(record_ptr), sizeof(WearLevelRecord));
                }
#endif

                if(record_ptr->magic == MAGIC_VALUE)
                {
                    // Use modular comparison (by casting subtraction to int32_t) to handle sequence number wrap
                    if(!found_any || static_cast<int32_t>(record_ptr->seq_num - max_seq) > 0)
                    {
                        max_seq = record_ptr->seq_num;
                        latest_sector_ = s;
                        latest_slot_ = slot;
                        settings_ = record_ptr->config;
                        found_any = true;
                    }
                }
            }
        }

        if(found_any)
        {
            max_seq_num_ = max_seq;
            has_valid_record_ = true;
        }
        else
        {
            // First time boot / completely erased flash
            WriteToSlot(0, 0, 1, default_settings_);
        }
    }

    /**
     * @brief Returns a reference to the active settings structure in RAM.
     */
    SettingStruct &GetSettings() { return settings_; }

    /**
     * @brief Saves the current settings to flash if they differ from the last persisted settings.
     */
    void Save()
    {
        // Check if anything changed compared to the last persisted slot.
        if(has_valid_record_)
        {
            uint32_t slot_offset = latest_sector_ * SECTOR_SIZE + latest_slot_ * sizeof(WearLevelRecord);
            auto* last_persisted = reinterpret_cast<WearLevelRecord*>(
                reinterpret_cast<uint8_t*>(qspi_.GetData(address_offset_)) + slot_offset
            );

#if !UNIT_TEST
            if(System::GetProgramMemoryRegion() != System::MemoryRegion::INTERNAL_FLASH)
            {
                dsy_dma_invalidate_cache_for_buffer(reinterpret_cast<uint8_t*>(last_persisted), sizeof(WearLevelRecord));
            }
#endif
            if(settings_ == last_persisted->config)
            {
                return; // Nothing changed, skip writing
            }
        }

        size_t next_sector = latest_sector_;
        size_t next_slot = latest_slot_ + 1;

        if(!has_valid_record_)
        {
            next_sector = 0;
            next_slot = 0;
        }
        else if(next_slot >= SLOTS_PER_SECTOR)
        {
            next_slot = 0;
            next_sector = (latest_sector_ + 1) % NumSectors;
        }

        uint32_t next_seq = has_valid_record_ ? (max_seq_num_ + 1) : 1;

        WriteToSlot(next_sector, next_slot, next_seq, settings_);
    }

    /**
     * @brief Restores the default configuration and saves it to the next slot.
     */
    void RestoreDefaults()
    {
        settings_ = default_settings_;
        Save();
    }

  private:
    static constexpr uint32_t MAGIC_VALUE = 0x5A5A1234;
    static constexpr uint32_t SECTOR_SIZE = 4096;

    struct alignas(4) WearLevelRecord
    {
        uint32_t magic;
        uint32_t seq_num;
        SettingStruct config;
    };

    static constexpr size_t SLOTS_PER_SECTOR = SECTOR_SIZE / sizeof(WearLevelRecord);

    void WriteToSlot(size_t sector, size_t slot, uint32_t seq_num, const SettingStruct &config)
    {
        uint32_t sector_addr = address_offset_ + sector * SECTOR_SIZE;
        uint32_t write_addr = sector_addr + slot * sizeof(WearLevelRecord);

        // Erase the target sector if we are writing to its first slot (slot 0)
        if(slot == 0)
        {
            qspi_.EraseSector(sector_addr);
        }

        WearLevelRecord record;
        record.magic = MAGIC_VALUE;
        record.seq_num = seq_num;
        record.config = config;

        qspi_.Write(write_addr, sizeof(WearLevelRecord), reinterpret_cast<uint8_t*>(&record));

        latest_sector_ = sector;
        latest_slot_ = slot;
        max_seq_num_ = seq_num;
        has_valid_record_ = true;
    }

    QSPIHandle &qspi_;
    uint32_t address_offset_;
    SettingStruct default_settings_;
    SettingStruct settings_;
    size_t latest_sector_;
    size_t latest_slot_;
    uint32_t max_seq_num_;
    bool has_valid_record_;
};

} // namespace daisy
