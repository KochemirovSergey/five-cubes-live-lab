#include "../unreal/FiveCubes/Source/FiveCubes/PcmResampler.h"
#include <cassert>
#include <iostream>
int main(){
 for(int Rate:{44100,48000}){
  FLabPcmResampler R;std::vector<int16_t> Output;
  for(int Offset=0;Offset<Rate*2;){const int Frames=std::min(257,Rate*2-Offset);std::vector<float> In(Frames*2);
   for(int I=0;I<Frames;++I)In[I*2]=In[I*2+1]=0.5f*std::sin(2*3.141592653589793*1000*(Offset+I)/Rate);
   R.Push(In.data(),Frames,2,Rate);auto Part=R.Drain();Output.insert(Output.end(),Part.begin(),Part.end());Offset+=Frames;
  }
  assert(!R.Overflow);assert(Output.size()>=47520 && Output.size()<=48000);
  int Crossings=0;for(size_t I=1;I<Output.size();++I)if(Output[I-1]<=0&&Output[I]>0)++Crossings;
  double Hz=Crossings*24000.0/Output.size();assert(std::abs(Hz-1000)<2);
 }
 FLabPcmResampler R;std::vector<float> Large(48000*3);R.Push(Large.data(),static_cast<int>(Large.size()),1,48000);assert(R.Overflow);
 R.Reset();float F[2]={1,1};R.Push(F,2,1,48000);R.Push(F,2,1,44100);assert(R.Overflow);
 std::cout<<"Resampler: 44.1/48kHz stereo -> 24kHz mono, duration, pitch, overflow, device rate change passed\n";
}
