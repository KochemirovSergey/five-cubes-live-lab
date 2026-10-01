#include "../unreal/FiveCubes/Source/FiveCubes/OberbeckModel.h"
#include <cassert>
#include <iostream>
static void near(double a,double b){assert(std::abs(a-b)<=1e-9+1e-6*std::abs(b));}
int main(){
 FOberbeckModel m;assert(m.P.Valid());m.Reset();m.H=.6;
 near(m.Inertia(),.003921666666666667);near(m.HitTime(),4.6306556797079805);
 for(int fps:{30,60,120}){m.Reset();m.H=.6;assert(m.Release());const double t=m.HitTime();for(int i=0;i<fps*10&&!m.Landed;i++){
  m.Advance(1.0/fps);const double v=m.Acceleration()*m.Elapsed(),w=v/m.Radius();near(m.P.M0*m.P.G*m.Displacement(),m.P.M0*v*v/2+m.Inertia()*w*w/2);near(m.Displacement(),(m.InitialAngle-m.Angle)*m.Radius());
 }assert(m.Landed);near(m.Elapsed(),t);near(m.CurrentHeight(),0);}
 m.Reset();m.H=.6;const double small=m.HitTime();m.SetR(.20);assert(m.HitTime()>small);m.Pulley=1;assert(m.HitTime()<small);
 m.Reset();assert(!m.Release());m.Wind(1e6);near(m.H,m.P.HMax);m.Wind(-1e6);near(m.H,0);
 m.H=.6;m.TimerToggle();m.Advance(.2);m.Release();const double end=m.HitTime();m.Advance(end+.3);assert(m.TimerRunning);assert(m.TimerToggle());near(m.Records[0].Time,end+.5);assert(m.Step==1);
 m.NewTrial();m.SetR(.16);m.H=.6;m.Release();m.TimerToggle();m.Advance(20);assert(m.TimerToggle());assert(m.Step==2);
 m.Reset();m.H=.6;m.Release();m.TimerToggle();m.Advance(.2);assert(!m.TimerToggle());assert(m.Early&&m.Records.empty());
 m.Reset();m.H=.6;m.Release();m.TimerToggle();m.Paused=true;m.Advance(100);near(m.Elapsed(),0);near(m.Stopwatch(),0);m.Paused=false;m.Advance(10);assert(m.Landed);
 m.Reset();m.H=.6;m.Release();m.SetR(.2);near(m.R,.12);m.Advance(100);m.NewTrial();assert(!m.Landed&&!m.Falling&&!m.TimerUsed);
 m.Reset();m.H=.6;m.Release();const double expected=m.HitTime();for(double d:{.01,.3,.02,1.1,.001,20.0})m.Advance(d);near(m.Elapsed(),expected);near(m.CurrentHeight(),0);
 m.Reset();m.H=.6;m.Release();m.TimerToggle();m.Advance(10);m.TimerToggle();m.NewTrial();m.H=.6;m.SetR(.125);m.Release();m.TimerToggle();m.Advance(10);assert(!m.TimerToggle());assert(m.Records.size()==1);
 m.NewTrial();m.H=.6;m.SetR(.16);m.Pulley=1;m.Release();m.TimerToggle();m.Advance(10);assert(!m.TimerToggle());
 m.NewTrial();m.Pulley=0;m.H=.65;m.Release();m.TimerToggle();m.Advance(10);assert(!m.TimerToggle());
 m.Reset();assert(m.Records.empty()&&m.Step==0);m.P.G=0;assert(!m.P.Valid());assert(!m.Release());
 std::cout<<"Oberbeck physics, energy, timing, comparisons and reset: OK\n";
}
