#pragma once
#include <cmath>
#include <string>
// Only user actions advance the exercise. Independent of rendering and networking.
struct FPanelProgress {
    int Step=0;
    float Angle=0, Travel=0, Threshold=5;
    bool Right=false, Up=false, CodeError=false;
    std::string Digits, Code="773";
    bool Done() const { return Step==4; }
    void Reset() { Step=0; Angle=Travel=0; Right=Up=CodeError=false; Digits.clear(); }
    void Rotate(float Delta) {
        Angle=std::fmod(Angle+Delta+360.f,360.f);
        if(Step==0) { Travel+=std::abs(Delta); if(Travel>=Threshold) ++Step; }
    }
    void Slide(bool ToRight) {
        bool Changed=Right!=ToRight; Right=ToRight;
        if(Step==1 && Changed && Right) { ++Step; Digits.clear(); CodeError=false; }
    }
    void Key(char Key) {
        if(Key=='C') { Digits.clear(); CodeError=false; }
        else if(Key=='E') {
            if(Step==2) { CodeError=Digits!=Code; if(!CodeError) ++Step; }
        } else if(Key>='0' && Key<='9' && Digits.size()<3) { Digits+=Key; CodeError=false; }
    }
    void Toggle() { Up=!Up; if(Step==3) ++Step; }
};
