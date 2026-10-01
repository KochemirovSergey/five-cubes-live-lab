#include "../unreal/FiveCubes/Source/FiveCubes/OberbeckGuidance.h"
#include <cassert>
#include <iostream>
int main(){
 FOberbeckLab L;FOberbeckModel P;P.Individual=true;
 auto next=[&](){return OberbeckNext(L,P);};
 assert(next().Id=="remove_mass"&&next().Target=="oberbeck.mass.1");
 P.Installed[0]=false;assert(next().Target=="oberbeck.mass.2");
 P.Installed.fill(false);assert(next().Field=="measure:D0");
 L.Measure("D0","30","mm",.03,true);assert(next().Id=="select_pulley");P.Pulley=1;
 assert(next().Field=="measure:D1");L.Measure("D1","60","mm",.06,true);
 assert(next().Target=="oberbeck.pulley.small");P.Pulley=0;assert(next().Id=="wind");P.H=.6;
 assert(next().Field=="measure:h");L.Measure("h","60","cm",.6,true);assert(next().Id=="open_series");
 assert(L.Begin(P,3));assert(next().Id=="release");P.Release();assert(next().Id=="start_timer");P.TimerToggle();assert(next().Id=="wait_for_landing");
 P.Paused=true;assert(next().Id=="resume");P.Paused=false;P.Advance(100);assert(next().Id=="stop_timer");P.TimerToggle();L.Capture(P,"pending");assert(next().Id=="copy_time");
 L.Enter(L.NextAttempt-1,std::to_string(L.Current()->Attempts.back().Displayed),"s");assert(next().Id=="new_trial");
 assert(OberbeckNext(L,P,true).Id=="review");
 P.NewTrial();P.Wind(100);L.Current()->Attempts.push_back({L.NextAttempt++,2.4,2.4,0,"accepted"});L.Current()->Attempts.push_back({L.NextAttempt++,2.4,2.4,0,"accepted"});assert(next().Id=="select_pulley");
 // Guidance must not change the workflow or produce reference values.
 auto Before=L.Revision;auto H=next();assert(L.Revision==Before&&H.Text.find("0.001")==std::string::npos);
 assert(!L.Measure("h","1","kg",.6,true)&&L.ErrorCode=="invalid_number_or_unit");
 assert(!L.Measure("h","60","cm",.6,false)&&L.ErrorCode=="instrument_alignment");
 assert(!L.Measure("h","59","cm",.6,true)&&L.ErrorCode=="reading_mismatch");
 // Isolate calculation stages with complete synthetic measurements.
 L.Series.clear();L.Active=-1;P.NewTrial();P.H=.6;L.Readings["R"]={.225,.001,"m"};
 for(int S=0;S<4;S++){FObSeries R;R.Id=S+1;R.Slot=S;double T=S<2?(S==0?2.36:1.22):(S==2?7.8:3.9);for(int I=0;I<3;I++)R.Attempts.push_back({S*3+I+1,T,T,0,"accepted"});L.Series.push_back(R);}
 auto Refs=L.References();
 for(int Task=0;Task<5;Task++){
  assert(L.Task()==Task);assert(next().Id=="calculate");
  for(const auto& R:Refs)if(R.second.Task==Task)L.Answers[R.first]={R.second.Value,R.second.Unit,true};
  if(Task==0){assert(next().Id=="conclusion");L.Conclusion0=true;}
  if(Task==1){assert(next().Id=="conclusion");L.Conclusion1=true;}
  if(Task==3){assert(next().Id=="conclusion");L.ConclusionRotation=true;}
  if(Task==4){assert(next().Id=="coefficients");L.Coefficients=true;}
 }
 assert(next().Id=="complete");
 std::cout<<"Guidance: preparations, attempts, all five tasks, errors and completion OK\n";
}
