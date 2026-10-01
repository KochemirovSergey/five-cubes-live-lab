#include "../unreal/FiveCubes/Source/FiveCubes/OberbeckLab.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <iomanip>
std::string number(double V){std::ostringstream O;O<<std::setprecision(16)<<V;return O.str();}
int main(){
 double X;assert(FOberbeckLab::Parse(" 2,25e1 ","cm",X)&&std::abs(X-.225)<1e-12);assert(!FOberbeckLab::Parse("nan","m",X));assert(!FOberbeckLab::Parse("1foo","m",X));assert(!FOberbeckLab::Parse("1e999","m",X));assert(!FOberbeckLab::Parse("2","bad",X));
 FOberbeckModel P;P.Individual=true;P.H=.6;P.SetMass(0,false,.225);assert(!P.Release());P.Installed.fill(false);assert(P.Inertia()==.001);assert(P.Release());P.NewTrial();P.Installed.fill(true);P.MassR[3]=.20;P.H=.6;assert(!P.Release());P.SetMass(3,true,.224);assert(P.Balanced());assert(std::abs(P.Inertia()-.0111666666666667)<1e-14);
 FOberbeckLab L;
 assert(!L.Measure("D0","30","mm",.03,false));assert(!L.Measure("D0","3","mm",.03,true));
 assert(L.Measure("D0","30","mm",.03,true));assert(L.Measure("D1","6","cm",.06,true));assert(L.Measure("h","60","cm",.6,true));assert(L.Measure("R","22.5","cm",.225,true));
 P.NewTrial();P.H=.6;assert(!L.Begin(P,3)); // loaded before unloaded
 int LastId=0;
 for(int Slot=0;Slot<4;Slot++){
  P.NewTrial();P.H=.6;P.Installed.fill(Slot>=2);P.MassR.fill(.225);P.Pulley=Slot%2;
  assert(L.Begin(P,3));P.H=.59;assert(!L.CanRelease(P));P.H=.6;
  if(!Slot){assert(L.CanRelease(P));assert(P.Release());P.TimerToggle();P.Advance(.1);assert(!P.TimerToggle());L.Capture(P,"early");assert(!L.Enter(L.NextAttempt-1,".1","s"));P.NewTrial();P.H=.6;}
  for(int I=0;I<3;I++){
   assert(L.CanRelease(P));assert(P.Release());assert(P.TimerToggle());P.Advance(P.HitTime()+.03*I+1e-9);assert(P.TimerToggle());L.Capture(P,"pending");LastId=L.NextAttempt-1;
   auto A=L.Current()->Attempts.back();assert(!L.Enter(A.Id,number(A.Displayed*1000),"s"));assert(L.Enter(A.Id,number(A.Displayed),"s"));assert(L.Current()->Count()==I+1);
   P.NewTrial();P.Wind(100);assert(std::abs(P.H-.6)<1e-12);
  }
  assert(L.Current()->Complete());assert(!L.CanRelease(P));
 }
 auto Refs=L.References();assert(Refs.size()>=40);assert(!Refs.count("approx_rel1"));
 // Independent finite-difference derivative check of exact conservative propagation.
 double m=.1,r=.015,h=.6,t=2.36,dt=.23;
 auto formula=[](double M,double R,double H,double T){return M*R*R*(9.81*T*T-2*H)/(2*H);};
 double eps=1e-7;
 double independent=std::abs((formula(m+eps,r,h,t)-formula(m-eps,r,h,t))/(2*eps))*.0005+std::abs((formula(m,r+eps,h,t)-formula(m,r-eps,h,t))/(2*eps))*.00005+std::abs((formula(m,r,h+eps,t)-formula(m,r,h-eps,t))/(2*eps))*.001+std::abs((formula(m,r,h,t+eps)-formula(m,r,h,t-eps))/(2*eps))*dt;
 assert(std::abs(independent-FOberbeckLab::JError(m,r,h,t,dt))<1e-10);
 assert(!L.Check("j0",number(Refs.at("j0").Value),"m"));
 assert(!L.Check("j0",number(Refs.at("j0").Value*1.02),"kg*m2")); // uncertainty must not widen arithmetic tolerance
 for(auto& R:Refs)assert(L.Check(R.first,number(R.second.Value),R.second.Unit));
 assert(L.Conclude(0,L.Agree(0)));assert(L.Conclude(1,L.Agree(1)));assert(L.Conclude(2,L.Agree(2)));L.Coefficients=true;assert(L.Task()==5);
 L.Exclude(LastId);assert(L.Task()==0);assert(!L.Slot(3)->Complete());assert(!L.Enter(LastId,"1","s"));assert(!L.Answers.at("j0").Correct);
 // independent analytical time for two configurations / both pulleys
 for(bool Loaded:{false,true})for(int Pulley:{0,1}){P.NewTrial();P.H=.6;P.Installed.fill(Loaded);P.Pulley=Pulley;double J=Loaded?.011166666666666667:.001;double Radius=Pulley?.03:.015;double Expected=std::sqrt(1.2*(.1+J/(Radius*Radius))/(.1*9.81));assert(std::abs(P.HitTime()-Expected)<1e-12);}
 // Choosing N is explicit, 2 and 6 are rejected, 4 and 5 accepted.
 P.NewTrial();P.H=.6;P.Installed.fill(false);assert(!L.Begin(P,2));assert(!L.Begin(P,6));assert(L.Begin(P,4));assert(L.Begin(P,5));
 std::cout<<"Full workflow, four series, guards, input units, uncertainty derivatives and invalidation: OK\n";
}
