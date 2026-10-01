#pragma once
#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include <atomic>
#include "LabPlaybackWave.generated.h"

// Meter actual PCM consumed by the renderer, not provider packet arrival.
// Device/DAC latency is deliberately outside this software measurement.
UCLASS()
class FIVECUBES_API ULabPlaybackWave : public USoundWaveProcedural {
    GENERATED_BODY()
public:
    std::atomic<double> LastNonSilent{0};
    virtual int32 GeneratePCMData(uint8* Data,const int32 SamplesNeeded)override {
        int32 Bytes=Super::GeneratePCMData(Data,SamplesNeeded);double Energy=0;
        for(int I=0;I+1<Bytes;I+=2){int16 Value;FMemory::Memcpy(&Value,Data+I,2);double V=Value/32768.;Energy+=V*V;}
        if(Bytes>0&&Energy/(Bytes/2)>.00001)LastNonSilent.store(FPlatformTime::Seconds(),std::memory_order_relaxed);
        return Bytes;
    }
};
