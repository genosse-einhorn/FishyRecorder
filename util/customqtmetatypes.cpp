#include "customqtmetatypes.h"
#include "recording/trackdataaccessor.h"

using TrackDataAccessor = Recording::TrackDataAccessor;

__attribute__((constructor))
void registerMetaTypes()
{
    qRegisterMetaType<PaDeviceIndex>("PaDeviceIndex");
    qRegisterMetaType<uint64_t>("uint64_t");
    qRegisterMetaType<int16_t>("int16_t");
    qRegisterMetaType<TrackDataAccessor*>("TrackDataAccessor*");
}
