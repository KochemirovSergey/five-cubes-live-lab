#pragma once
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstdint>
// Approximate speech activity for instrumentation only. Never gates the microphone
// or cancels output. Thresholds and silence window are recorded in the report.
struct FVoiceTiming {
    bool Speaking=false;int LoudFrames=0;double LastInput=0,Onset=0,End=0,Interruption=0;
    bool Input(const int16_t* Samples,size_t Count,double Now,double LastOutput){
        double E=0;for(size_t I=0;I<Count;I++){double V=Samples[I]/32768.;E+=V*V;}
        bool Loud=Count&&E/Count>.000225;bool Started=false;
        if(Loud){LastInput=Now;if(++LoudFrames>=3&&!Speaking){Speaking=true;Onset=Now-.04;End=0;Started=true;if(LastOutput>0&&Now-LastOutput<.15)Interruption=Onset;}}
        else {LoudFrames=0;if(Speaking&&Now-LastInput>=.3){Speaking=false;End=LastInput;}}
        return Started;
    }
    // >0 elapsed ms; -1 timeout; 0 not ready. A 200 ms renderer silence is required.
    double Poll(double Now,double LastOutput){
        if(!Interruption)return 0;
        if(Now-LastOutput>=.2){double Ms=std::max(1.,(std::max(Interruption,LastOutput)-Interruption)*1000);Interruption=0;return Ms;}
        if(Now-Interruption>=3){Interruption=0;return -1;}
        return 0;
    }
};
