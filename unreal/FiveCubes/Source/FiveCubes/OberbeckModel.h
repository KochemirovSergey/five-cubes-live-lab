#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <array>

// SI units. Rendering and network latency never participate in integration.
struct FOberbeckProfile {
    double G=9.81, M0=.10, M=.05, Width=.04, Height=.03, Depth=.03;
    double J0=.001, Small=.015, Large=.03, RMin=.08, RMax=.20, RDefault=.12;
    double HMin=.20, HMax=.80, HDefault=.60, Spoke=.25;
    bool Valid() const {
        const double Values[]={G,M0,M,Width,Height,Depth,J0,Small,Large,RMin,RMax,RDefault,HMin,HMax,HDefault,Spoke};
        for(double V:Values)if(!std::isfinite(V)||V<=0)return false;
        return Small<Large && RMin<=RDefault && RDefault<=RMax && RMax+Width/2<=Spoke && HMin<=HDefault && HDefault<=HMax;
    }
};
struct FOberbeckRecord { double R,H,Time; int Pulley; };
struct FOberbeckModel {
    FOberbeckProfile P;
    double R=.12,H=0,Angle=0,Clock=0,ReleasedAt=0,InitialAngle=0,TimerAt=0,TimerValue=0;
    int Pulley=0,Step=0;
    bool Falling=false,Landed=false,TimerRunning=false,TimerUsed=false,Paused=false,Early=false;
    std::vector<FOberbeckRecord> Records;
    bool Individual=false;
    std::array<bool,4> Installed{{true,true,true,true}};
    std::array<double,4> MassR{{.225,.225,.225,.225}};
    double HeightStop=0;
    int MassCount() const {return std::count(Installed.begin(),Installed.end(),true);}
    bool Balanced() const {if(!Individual)return true;if(MassCount()==0)return true;if(MassCount()!=4)return false;for(double V:MassR)if(std::abs(V-MassR[0])>1e-6)return false;return true;}
    void SetMass(int I,bool On,double V){if(!Editable()||I<0||I>3||!std::isfinite(V))return;Installed[I]=On;MassR[I]=std::clamp(V,P.RMin,.23);for(int J=0;J<4;J++)if(J!=I&&Installed[J]&&std::abs(MassR[I]-MassR[J])<=.003){MassR[I]=MassR[J];break;}}
    double Radius() const{return Pulley?P.Large:P.Small;}
    double Inertia() const{double J=P.J0;for(int I=0;I<4;I++)if(!Individual||Installed[I]){double V=Individual?MassR[I]:R;J+=P.M*(P.Width*P.Width+P.Height*P.Height)/12+P.M*V*V;}return J;}
    double Acceleration() const{return P.M0*P.G/(P.M0+Inertia()/(Radius()*Radius()));}
    double HitTime() const{return std::sqrt(2*H/Acceleration());}
    double Elapsed() const{return std::clamp(Clock-ReleasedAt,0.0,HitTime());}
    double Displacement() const{return (Falling||Landed)?Acceleration()*Elapsed()*Elapsed()/2:0;}
    double CurrentHeight() const{return std::max(0.0,H-Displacement());}
    double Stopwatch() const{return TimerRunning?Clock-TimerAt:TimerValue;}
    void Reset(){auto Profile=P;*this=FOberbeckModel();P=Profile;R=P.RDefault;}
    bool Editable()const{return !Falling&&!Landed&&!TimerRunning&&!Paused;}
    void SetR(double V){if(Editable()&&std::isfinite(V))R=std::clamp(V,P.RMin,P.RMax);}
    void Wind(double Delta){if(Editable()&&std::isfinite(Delta)){double Next=std::clamp(H+Delta*Radius(),0.0,HeightStop>0?HeightStop:P.HMax);Angle+=(Next-H)/Radius();H=Next;}}
    bool Release(){if(!Balanced()||!P.Valid()||Falling||Landed||Paused||H<P.HMin)return false;Falling=true;ReleasedAt=Clock;InitialAngle=Angle;return true;}
    void Advance(double D){if(Paused||!std::isfinite(D)||D<0)return;Clock+=D;if(Falling){Angle=InitialAngle-Displacement()/Radius();if(Clock-ReleasedAt>=HitTime()){Falling=false;Landed=true;Angle=InitialAngle-H/Radius();}}}
    bool TimerToggle(){
        if(Paused)return false;
        if(!TimerRunning){if(TimerUsed||Landed)return false;TimerAt=Clock;TimerRunning=true;TimerUsed=true;return true;}
        TimerValue=Stopwatch();TimerRunning=false;
        if(!Landed){Early=true;return false;}
        if(Individual)return true;
        if(Records.empty()){Records.push_back({R,H,TimerValue,Pulley});Step=1;return true;}
        const auto& First=Records.front();
        if(Records.size()==1&&std::abs(R-First.R)>=.02-1e-9&&std::abs(H-First.H)<=.005+1e-9&&Pulley==First.Pulley){Records.push_back({R,H,TimerValue,Pulley});Step=2;return true;}
        return false;
    }
    void ResetTimer(){if(!Falling&&!Landed){TimerRunning=false;TimerUsed=false;TimerValue=0;Early=false;}}
    void NewTrial(){if(Records.size()==2){Records.pop_back();Step=1;}Falling=Landed=TimerRunning=TimerUsed=Early=Paused=false;TimerValue=0;H=0;Angle=0;}
};
