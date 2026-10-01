#include "../unreal/FiveCubes/Source/FiveCubes/VoiceTiming.h"
#include <cassert>
#include <iostream>
int main(){FVoiceTiming T;int16_t Loud[480],Quiet[480]={};for(auto& V:Loud)V=2000;
 T.Input(Quiet,480,1,0);assert(!T.Speaking&&!T.End);
 T.Input(Loud,480,1.02,1.01);T.Input(Loud,480,1.04,1.03);assert(T.Input(Loud,480,1.06,1.05));assert(T.Speaking&&T.Interruption>0);
 assert(!T.Poll(1.2,1.15));double Ms=T.Poll(1.4,1.15);assert(Ms>129&&Ms<131);assert(!T.Poll(2,1.15));
 T.Input(Quiet,480,1.4,1.15);assert(!T.Speaking&&T.End==1.06);
 T.Input(Loud,480,2.02,2.01);T.Input(Loud,480,2.04,2.03);T.Input(Loud,480,2.06,2.05);assert(T.Poll(5.1,5.09)==-1);
 std::cout<<"Voice timing: onset/end, renderer silence, timeout and no duplicate sample OK\n";}
