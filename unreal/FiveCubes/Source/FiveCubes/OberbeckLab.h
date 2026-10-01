#pragma once
#include "OberbeckModel.h"
#include <map>
#include <string>
#include <cstdlib>
#include <cerrno>

// Portable workflow and reference calculator. All numeric data are SI.
struct FObReading { double Value=0, Error=0; std::string Unit; };
struct FObAttempt {
    int Id=0; double Displayed=0, Entered=0, InitialAngle=0;
    std::string Status="pending"; // accepted, excluded, early, interrupted
};
struct FObSeries {
    int Id=0, Slot=0, N=3; double H=0, R=0, J=0, Radius=0, Acceleration=0;
    std::vector<FObAttempt> Attempts;
    int Count()const {return std::count_if(Attempts.begin(),Attempts.end(),[](const FObAttempt& A){return A.Status=="accepted";});}
    bool Complete()const {return Count()==N;}
    double Mean()const {double Sum=0;for(const auto& A:Attempts)if(A.Status=="accepted")Sum+=A.Entered;return Count()?Sum/Count():0;}
    double TimeError()const {double Spread=0;for(const auto& A:Attempts)if(A.Status=="accepted")Spread=std::max(Spread,std::abs(A.Entered-Mean()));return Spread+.205;}
};
struct FObAnswer {double Value=0;std::string Unit;bool Correct=false;};
struct FObReference {double Value;std::string Unit, Label, Formula;int Task;};
struct FOberbeckLab {
    static constexpr int Format=1, Method=1, Profile=1;
    std::vector<FObSeries> Series;
    std::map<std::string,FObReading> Readings;
    std::map<std::string,FObAnswer> Answers;
    std::vector<std::string> History;
    int Active=-1, NextSeries=1, NextAttempt=1, Revision=0;
    double FixedHeight=0;
    bool Coefficients=false, Conclusion0=false,Conclusion1=false,ConclusionRotation=false;
    std::string Error, ErrorCode;
    static double Rounded(double V,double Step){return std::round(V/Step)*Step;}
    static bool Parse(std::string Text,const std::string& Unit,double& V){
        for(char& C:Text)if(C==',')C='.';
        const std::map<std::string,double> Units={{"m",1},{"cm",.01},{"mm",.001},{"s",1},{"kg",1},{"g",.001},{"kg*m2",1},{"g*cm2",1e-7},{"m/s2",1},{"rad/s2",1},{"N",1},{"N*m",1},{"1",1},{"%",.01}};
        auto U=Units.find(Unit);if(U==Units.end())return false;
        char* End=nullptr;errno=0;const char* Start=Text.c_str();V=std::strtod(Start,&End);
        if(Start==End||errno==ERANGE)return false;while(*End==' '||*End=='\t')End++;
        V*=U->second;return !*End&&std::isfinite(V);
    }
    void Changed(const std::string& Event){Revision++;History.push_back(Event);for(auto& A:Answers)A.second.Correct=false;Conclusion0=Conclusion1=ConclusionRotation=false;}
    double Reading(const std::string& K)const {auto I=Readings.find(K);return I==Readings.end()?0:I->second.Value;}
    bool Measure(const std::string& K,const std::string& Text,const std::string& Unit,double Displayed,bool Positioned){
        double V=0;double Resolution=(K=="D0"||K=="D1")?.0001:.001;
        if((Unit!="m"&&Unit!="cm"&&Unit!="mm")||!Parse(Text,Unit,V)||V<=0){ErrorCode="invalid_number_or_unit";Error="Введите положительную длину и выберите единицу.";return false;}
        if(!Positioned){ErrorCode="instrument_alignment";Error="Сначала совместите измерительный курсор с краем или центром детали.";return false;}
        if(std::abs(V-Displayed)>Resolution*.501){ErrorCode="reading_mismatch";Error="Ввод не совпадает с отсчётом прибора. Проверьте число и единицы.";return false;}
        Readings[K]={V,Resolution,Unit};Changed("measurement:"+K+"="+std::to_string(V)+" SI");Error="Показание записано; зависимые расчёты требуют проверки.";return true;
    }
    const FObSeries* Slot(int K)const {for(auto I=Series.rbegin();I!=Series.rend();++I)if(I->Slot==K)return &*I;return nullptr;}
    FObSeries* Current(){return Active>=0&&Active<(int)Series.size()?&Series[Active]:nullptr;}
    bool Begin(FOberbeckModel& P,int N){
        if(!P.Editable()||!P.Balanced()||P.H<P.P.HMin||N<3||N>5){ErrorCode="configuration";Error="Подготовьте симметричную установку, высоту от 20 см и 3–5 повторов.";return false;}
        bool Loaded=P.MassCount()==4;int K=(Loaded?2:0)+P.Pulley;
        if(Loaded&&(P.MassR[0]<.22||P.MassR[0]>.23)){ErrorCode="configuration";Error="Установите все грузы на одинаковом R от 22 до 23 см.";return false;}
        if(!Reading("D0")||!Reading("D1")||!Reading("h")||(Loaded&&!Reading("R"))){ErrorCode="missing_measurement";Error="Сначала измерьте оба диаметра, высоту и (для грузов) R.";return false;}
        if(Loaded&&(!Slot(0)||!Slot(0)->Complete()||!Slot(1)||!Slot(1)->Complete())){ErrorCode="incomplete_series";Error="Сначала завершите обе серии без грузов.";return false;}
        if(std::abs(Reading("h")-P.H)>.0011||(Loaded&&std::abs(Reading("R")-P.MassR[0])>.0011)){ErrorCode="measurement_stale";Error="Измерения h или R не соответствуют подготовке. Повторите измерение.";return false;}
        if(FixedHeight&&std::abs(P.H-FixedHeight)>1e-6){ErrorCode="height_stop";Error="Намотайте нить до установленного ограничителя высоты.";return false;}
        if(!FixedHeight)FixedHeight=P.H;P.HeightStop=FixedHeight;
        FObSeries S;S.Id=NextSeries++;S.Slot=K;S.N=N;S.H=P.H;S.R=Loaded?P.MassR[0]:0;S.J=P.Inertia();S.Radius=P.Radius();S.Acceleration=P.Acceleration();
        Series.push_back(S);Active=(int)Series.size()-1;Changed("series:"+std::to_string(S.Id));Error="Серия открыта. Отпустите подвес и отдельно запустите секундомер.";return true;
    }
    bool CanRelease(const FOberbeckModel& P){
        auto* S=Current();if(!S||S->Complete()){ErrorCode="missing_series";Error="Подготовьте и откройте новую серию.";return false;}
        if(!S->Attempts.empty()&&S->Attempts.back().Status=="pending"){ErrorCode="pending_attempt";Error="Перенесите время предыдущей попытки в журнал или исключите её.";return false;}
        if(!P.Balanced()||std::abs(S->J-P.Inertia())>1e-10||std::abs(S->H-P.H)>1e-6||std::abs(S->Radius-P.Radius())>1e-9){ErrorCode="configuration_changed";Error="Условия изменены. Восстановите их или явно откройте новую серию.";return false;}
        return true;
    }
    void Capture(const FOberbeckModel& P,const std::string& Status){auto* S=Current();if(!S)return;S->Attempts.push_back({NextAttempt++,Rounded(P.Stopwatch(),.01),0,P.InitialAngle,Status});Revision++;History.push_back("attempt:"+std::to_string(NextAttempt-1)+":"+Status);}
    bool Enter(int Id,const std::string& Text,const std::string& Unit){
        double V=0;if(Unit!="s"||!Parse(Text,Unit,V)||V<=0){ErrorCode="invalid_number_or_unit";Error="Время вводится положительным числом в секундах.";return false;}
        for(auto& S:Series)for(auto& A:S.Attempts)if(A.Id==Id){
            if(A.Status!="pending"&&A.Status!="accepted"){ErrorCode="invalid_attempt";Error="Неуспешная или исключённая попытка не принимается. Выполните повтор.";return false;}
            if(std::abs(V-A.Displayed)>.00501){ErrorCode="reading_mismatch";Error="Перенесите показание секундомера без изменений.";return false;}
            if(A.Status!="accepted"&&S.Count()>=S.N){ErrorCode="series_complete";Error="Серия уже заполнена.";return false;}
            A.Entered=V;A.Status="accepted";Changed("time:"+std::to_string(Id)+"="+std::to_string(V));Error="Время принято.";return true;
        }return false;
    }
    void Exclude(int Id){for(auto& S:Series)for(auto& A:S.Attempts)if(A.Id==Id){A.Status="excluded";Changed("exclude:"+std::to_string(Id));}}
    static double J(double M,double R,double H,double T,double G=9.81){return M*R*R*(G*T*T/(2*H)-1);}
    static double JError(double M,double R,double H,double T,double DT,double G=9.81){double B=G*T*T/(2*H)-1;return R*R*std::abs(B)*.0005+2*M*R*std::abs(B)*.00005+M*R*R*G*T/H*DT+M*R*R*G*T*T/(2*H*H)*.001;}
    std::map<std::string,FObReference> References()const {
        std::map<std::string,FObReference> Out;
        auto Add=[&](std::string K,double V,std::string U,std::string L,std::string F,int Task){if(std::isfinite(V)&&V>0)Out[K]={V,U,L,F,Task};};
        double H=Reading("h"),R=Reading("R");
        for(int I=0;I<2;I++)if(Reading("D"+std::to_string(I)))Add("r"+std::to_string(I),Reading("D"+std::to_string(I))/2,"m","Радиус шкива "+std::to_string(I+1),"r = D / 2",0);
        for(int I=0;I<4;I++){
            auto S=Slot(I);if(!S||!S->Complete()||!H)continue;double T=S->Mean(),r=Reading("D"+std::to_string(I%2))/2;if(!r)continue;
            auto K=std::to_string(I);std::string Caption=std::string(I<2?"без грузов, шкив ":"с грузами, шкив ")+std::to_string(I%2+1);int Task=I/2;double Inertia=J(.1,r,H,T);if(Inertia<=0)continue;
            Add("t"+K,T,"s","Среднее t — "+Caption,"t̄ = Σt / N",Task);
            Add("j"+K,Inertia,"kg*m2","J — "+Caption,"J = m₀ r² (g t̄² / (2h) − 1)",Task);
            double DJ=JError(.1,r,H,T,S->TimeError());
            Add("dt"+K,S->TimeError(),"s","Δt — "+Caption,"Δt = max|tᵢ − t̄| + 0,20 + 0,005 с",4);
            Add("dj"+K,DJ,"kg*m2","ΔJ — "+Caption,"Σ|∂J/∂x| Δx; x = m₀, r, t̄, h",4);
            if(I>=2){double A=2*H/(T*T),Alpha=A/r,Tension=.1*(9.81-A),M=Tension*r;
                Add("a"+K,A,"m/s2","a — "+Caption,"a = 2h / t̄²",3);Add("alpha"+K,Alpha,"rad/s2","α — "+Caption,"α = a / r",3);Add("tension"+K,Tension,"N","Натяжение — "+Caption,"T = m₀(g − a)",3);Add("M"+K,M,"N*m","Момент — "+Caption,"M = T r",3);Add("ratio"+K,M/Alpha,"kg*m2","M / α — "+Caption,"M / α",3);
            }
            if(I%2==0&&2*H/(T*T)/9.81<=.05){double Rel=.0005/.1+2*.00005/r+2*S->TimeError()/T+.001/H;Add("approx_rel"+K,Rel,"1","Приближённая ΔJ/J, — "+Caption,"Δm₀/m₀ + 2Δr/r + 2Δt/t̄ + Δh/h",4);Add("approx_abs"+K,.1*r*r*9.81*T*T/(2*H)*Rel,"kg*m2","Приближённая ΔJ, — "+Caption,"Jприбл × (ΔJ/J)прибл; a/g ≤ 0,05",4);}
        }
        for(int I=0;I<2;I++){auto A=Out.find("j"+std::to_string(I*2)),B=Out.find("j"+std::to_string(I*2+1));if(A!=Out.end()&&B!=Out.end()){Add("mean"+std::to_string(I),(A->second.Value+B->second.Value)/2,"kg*m2",I?"Среднее J₁":"Среднее J₀","(Jмал + Jбол) / 2",I);double E=(Out.at("dj"+std::to_string(I*2)).Value+Out.at("dj"+std::to_string(I*2+1)).Value)/2;Add("error_mean"+std::to_string(I),E,"kg*m2",I?"Δ среднего J₁":"Δ среднего J₀","(ΔJмал + ΔJбол) / 2",4);Add("rel_mean"+std::to_string(I),E/Out.at("mean"+std::to_string(I)).Value,"1",I?"Относительная Δ среднего J₁":"Относительная Δ среднего J₀","Δ среднего J / среднее J",4);}}
        if(Out.count("mean0")&&Out.count("mean1")&&R){Add("mass_j",(Out.at("mean1").Value-Out.at("mean0").Value)/4,"kg*m2","Инерция одного груза","(среднее J₁ − среднее J₀) / 4",2);Add("point_j",.05*R*R,"kg*m2","Приближение точечного груза","mR²; конечный груз также имеет собственную инерцию",2);}
        return Out;
    }
    bool Check(const std::string& K,const std::string& Text,const std::string& Unit){
        auto Refs=References();auto It=Refs.find(K);double V=0;
        if(It==Refs.end()){ErrorCode="insufficient_data";Error="Недостаточно данных или J ≤ 0. Проверьте измерения и повторите опыт.";return false;}
        bool Dimension=Unit==It->second.Unit||(It->second.Unit=="m"&&(Unit=="cm"||Unit=="mm"))||(It->second.Unit=="kg*m2"&&Unit=="g*cm2")||(It->second.Unit=="1"&&Unit=="%");
        if(!Dimension||!Parse(Text,Unit,V)){ErrorCode="invalid_number_or_unit";Error="Проверьте размерность, единицы и число.";return false;}
        bool Ok=std::abs(V-It->second.Value)<=std::max(std::abs(It->second.Value)*.005,1e-12);Answers[K]={V,Unit,Ok};Revision++;History.push_back("answer:"+K+"="+std::to_string(V)+(Ok?":accepted":":retry"));ErrorCode=Ok?"":"calculation";Error=Ok?"Верно.":"Расчёт не совпал. Проверьте формулу, единицы и округление (0,5%).";return Ok;
    }
    bool Agree(int Pair)const{auto R=References();int I=Pair==0?0:2;if(!R.count("j"+std::to_string(I))||!R.count("j"+std::to_string(I+1)))return false;return std::abs(R.at("j"+std::to_string(I)).Value-R.at("j"+std::to_string(I+1)).Value)<=R.at("dj"+std::to_string(I)).Value+R.at("dj"+std::to_string(I+1)).Value;}
    bool Conclude(int Pair,bool Within){if(!Slot(Pair==0?0:2)||!Slot(Pair==0?1:3)||!Slot(Pair==0?0:2)->Complete()||!Slot(Pair==0?1:3)->Complete())return false;bool Ok=Agree(Pair)==Within;if(Pair==0)Conclusion0=Ok;else if(Pair==1)Conclusion1=Ok;else ConclusionRotation=Ok;Revision++;ErrorCode=Ok?"":"conclusion";Error=Ok?"Вывод соответствует данным.":"Сравните разность оценок с суммой абсолютных погрешностей.";return Ok;}
    int Task()const {
        auto Refs=References();
        const std::vector<std::vector<std::string>> Required={{"r0","r1","t0","t1","j0","j1","mean0"},{"t2","t3","j2","j3","mean1"},{"mass_j","point_j"},{"a2","a3","alpha2","alpha3","tension2","tension3","M2","M3","ratio2","ratio3"},{"dt0","dt1","dt2","dt3","dj0","dj1","dj2","dj3","error_mean0","error_mean1","rel_mean0","rel_mean1","approx_rel0","approx_abs0","approx_rel2","approx_abs2"}};
        for(int T=0;T<5;T++){
            for(const auto& K:Required[T]){if(T==4&&K.find("approx_")==0&&!Refs.count(K))continue;auto A=Answers.find(K);if(!Refs.count(K)||A==Answers.end()||!A->second.Correct)return T;}
            if((T==0&&!Conclusion0)||(T==1&&!Conclusion1)||(T==3&&!ConclusionRotation)||(T==4&&!Coefficients))return T;
        }return 5;
    }
};
