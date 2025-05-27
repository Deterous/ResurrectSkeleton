module;

#include <cstdint>

export module cd.common;

import cd.cd;

namespace gpsxre
{

export int32_t lba_to_sample(int32_t lba, int32_t offset)
{
    return lba * CD_DATA_SIZE_SAMPLES + offset;
}

}