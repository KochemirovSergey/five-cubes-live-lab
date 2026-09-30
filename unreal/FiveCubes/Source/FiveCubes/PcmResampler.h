#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
// Stateful converter: channel downmix, anti-alias filtering and fractional clock.
// The capture callback and consumer must share the caller's lock.
class FLabPcmResampler
{
    std::vector<float> Samples;
    double Position=0;
    int Rate=0;
    double Filter1=0,Filter2=0;
public:
    float Level=0;
    bool Overflow=false;
    void Reset(){Samples.clear();Position=0;Rate=0;Filter1=Filter2=0;Level=0;Overflow=false;}
    void Push(const float* Data,int Frames,int Channels,int SampleRate){
        if(SampleRate<8000||Channels<1||Frames<0){Overflow=true;return;}
        if(Rate && Rate!=SampleRate){Overflow=true;return;}
        Rate=SampleRate;
        if(Samples.size()+Frames>static_cast<size_t>(Rate*2)){Overflow=true;return;}
        Level=0;const double A=1-std::exp(-2*3.141592653589793*8500/Rate);
        for(int I=0;I<Frames;++I){double V=0;for(int C=0;C<Channels;++C)V+=Data[I*Channels+C];V/=Channels;
            Level=std::max(Level,static_cast<float>(std::abs(V)));
            if(Rate>24000){Filter1+=A*(V-Filter1);Filter2+=A*(Filter1-Filter2);V=Filter2;}
            Samples.push_back(static_cast<float>(V));
        }
    }
    std::vector<int16_t> Drain(){
        std::vector<int16_t> Out;if(!Rate)return Out;
        const double Step=static_cast<double>(Rate)/24000;
        while(Position+Step*480+1<Samples.size())for(int I=0;I<480;++I){
            const size_t P=static_cast<size_t>(Position);const double V=Samples[P]+(Samples[P+1]-Samples[P])*(Position-P);
            Out.push_back(static_cast<int16_t>(std::clamp(std::lround(V*32767),-32768L,32767L)));Position+=Step;
        }
        const size_t Used=static_cast<size_t>(Position);
        Samples.erase(Samples.begin(),Samples.begin()+Used);Position-=Used;return Out;
    }
};
