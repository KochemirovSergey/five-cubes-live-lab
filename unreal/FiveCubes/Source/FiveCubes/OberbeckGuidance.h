#pragma once
#include "OberbeckLab.h"

struct FObHint { std::string Id, Text, Target, Field; };
// Recommendations are pure: no model controls, progress changes or highlights.
inline FObHint OberbeckNext(const FOberbeckLab& L,const FOberbeckModel& P,bool Replay=false){
    auto H=[](std::string Id,std::string Text,std::string Target,std::string Field=""){return FObHint{Id,Text,Target,Field};};
    if(Replay)return H("review","Завершите просмотр записи, чтобы продолжить измерения.","oberbeck.review");
    if(P.Paused)return H("resume","Нажмите «Продолжить после паузы».","oberbeck.resume");
    if(P.Falling){if(!P.TimerRunning&&!P.Early)return H("start_timer","Запустите отдельный секундомер кнопкой «Старт / стоп» или пробелом.","oberbeck.timer.toggle");return H("wait_for_landing","Дождитесь касания подвесом основания.","oberbeck.hanger");}
    if(P.TimerRunning)return H("stop_timer","Остановите секундомер после касания основания.","oberbeck.timer.toggle");
    const FObSeries* Active=L.Active>=0&&L.Active<(int)L.Series.size()?&L.Series[L.Active]:nullptr;
    if(Active&&!Active->Attempts.empty()&&Active->Attempts.back().Status=="pending")return H("copy_time","Перенесите показание секундомера в журнал и нажмите «Проверить».","oberbeck.attempt.input","time:"+std::to_string(Active->Attempts.back().Id));
    int Task=L.Task();if(Task==5)return H("complete","Все пять заданий выполнены. Можно экспортировать протокол из журнала.","oberbeck.export");
    int Needed=-1;for(int I=0;I<4;I++)if(!L.Slot(I)||!L.Slot(I)->Complete()){Needed=I;break;}
    // Complete the measurements needed by the current assignment before calculations.
    if(Needed>=0&&(Task>=1||Needed<2||L.Slot(2)||L.Slot(3))){
        if(P.Landed||P.Early)return H("new_trial","Нажмите «Новый опыт», затем поднимите подвес для повтора.","oberbeck.new_trial");
        bool Loaded=Needed>=2;
        for(int I=0;I<4;I++)if(P.Installed[I]!=Loaded)return H(Loaded?"install_mass":"remove_mass",Loaded?"Верните груз №"+std::to_string(I+1)+" из лотка на его спицу.":"Снимите груз №"+std::to_string(I+1)+" в одноимённое место лотка слева.","oberbeck.mass."+std::to_string(I+1));
        if(Loaded&&(!P.Balanced()||P.MassR[0]<.22||P.MassR[0]>.23))return H("balance","Установите четыре груза на одинаковом расстоянии 22–23 см от оси до центра груза.","oberbeck.mass.1");
        for(int I=0;I<2;I++)if(!L.Reading("D"+std::to_string(I))){if(P.Pulley!=I)return H("select_pulley","Выберите "+std::string(I?"большой":"малый")+" шкив.",I?"oberbeck.pulley.large":"oberbeck.pulley.small");return H("measure_diameter","Совместите губку штангенциркуля с краем шкива и введите диаметр с единицей.","oberbeck.input.D"+std::to_string(I),"measure:D"+std::to_string(I));}
        if(P.Pulley!=Needed%2)return H("select_pulley","Выберите "+std::string(Needed%2?"большой":"малый")+" шкив для следующей серии.",Needed%2?"oberbeck.pulley.large":"oberbeck.pulley.small");
        if(P.H<P.P.HMin||(L.FixedHeight&&std::abs(P.H-L.FixedHeight)>1e-6))return H("wind","Вращайте маховик за спицу: поднимите подвес; при установленном ограничителе — до упора.","oberbeck.spoke.1");
        if(!L.Reading("h")||std::abs(L.Reading("h")-P.H)>.0011)return H("measure_height","Измерьте высоту до нижней грани подвеса и введите отсчёт.","oberbeck.input.h","measure:h");
        if(Loaded&&(!L.Reading("R")||std::abs(L.Reading("R")-P.MassR[0])>.0011))return H("measure_radius","Измерьте расстояние от оси до центра груза №1 и введите R.","oberbeck.input.R","measure:R");
        if(!Active||Active->Slot!=Needed||Active->Complete())return H("open_series","Выберите 3–5 повторов и нажмите «Открыть новую серию».","oberbeck.series.open");
        if(std::abs(Active->J-P.Inertia())>1e-10||std::abs(Active->H-P.H)>1e-6||std::abs(Active->Radius-P.Radius())>1e-9)return H("configuration_changed","Условия активной серии изменились. Восстановите их или явно откройте новую серию; предыдущая останется в истории.","oberbeck.series.open");
        return H("release","Нажмите «Отпустить», затем отдельно запустите секундомер.","oberbeck.release");
    }
    auto Refs=L.References();for(const auto& R:Refs)if(R.second.Task==Task){auto A=L.Answers.find(R.first);if(A==L.Answers.end()||!A->second.Correct)return H("calculate","Задание "+std::to_string(Task+1)+": "+R.second.Label+". Используйте принятые измерения и проверьте единицы.","oberbeck.answer.input","answer:"+R.first);}
    if((Task==0&&!Refs.count("mean0"))||(Task==1&&!Refs.count("mean1")))return H("check_measurements","Проверьте высоту и времена: по принятым данным не получается положительный J. Повторите нужную серию.","oberbeck.journal");
    if(Task==0||Task==1||Task==3)return H("conclusion","Сравните разность двух оценок с суммой абсолютных погрешностей и выберите вывод.","oberbeck.conclusion");
    if(Task==4&&!L.Coefficients)return H("coefficients","Определите коэффициенты погрешностей по степеням величин в приближённой формуле.","oberbeck.coefficients","coefficients");
    return H("check_measurements","Недостаточно данных для положительного J. Проверьте измерения и повторите нужную серию.","oberbeck.journal");
}
