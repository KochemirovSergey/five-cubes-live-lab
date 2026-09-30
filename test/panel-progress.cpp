#include "../unreal/FiveCubes/Source/FiveCubes/PanelProgress.h"
#include <cassert>
#include <iostream>
int main(){
 FPanelProgress p;
 p.Slide(true);p.Key('7');p.Key('7');p.Key('3');p.Key('E');p.Toggle();assert(p.Step==0);
 p.Rotate(2);p.Rotate(-2);assert(p.Step==0);p.Rotate(1);assert(p.Step==1);
 p.Slide(true);assert(p.Step==1);p.Slide(false);p.Slide(true);assert(p.Step==2&&p.Digits.empty());
 p.Key('1');p.Key('E');assert(p.Step==2&&p.CodeError);p.Key('C');assert(p.Digits.empty()&&!p.CodeError);
 p.Key('7');p.Key('7');p.Key('3');p.Key('9');assert(p.Digits=="773");assert(p.Step==2);
 p.Key('E');assert(p.Step==3);p.Key('E');assert(p.Step==3);p.Toggle();assert(p.Done());
 p.Toggle();p.Rotate(-30);p.Slide(false);assert(p.Done());p.Reset();assert(p.Step==0&&!p.Right&&!p.Up&&p.Digits.empty()&&p.Angle==0);
 std::cout<<"Panel: ordered actions, early actions, jitter, wrong code, repeat Enter, completion, reset passed\n";
}
