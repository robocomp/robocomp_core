/****************************************************************************
* File name: GRAFCETStep.h
* Author: Alejandro Torrejon Harto
* Date: 16/10/2023
* Description: QState wrapper to execute functions, simulating a GRAFCET or SFC(Sequential Function Chart) step (according to EN61131-3).
****************************************************************************/

#ifndef GRAFCETSTEP_H
#define GRAFCETSTEP_H
#define DEBUG 0

#include <QState>
#include <QTimer>
#include <iostream>
#include <mutex>


/**
 * @class GRAFCETStep
 * @brief Class representing a state or step of the GRAFCET diagram.
 *
 * QState wrapper to execute functions, simulating a GRAFCET or SFC(Sequential Function Chart) step (according to EN61131-3).
 */
class GRAFCETStep : public QState
{
    Q_OBJECT

public:
    explicit GRAFCETStep(QString name, int period_ms = 100, const std::function<void()>& N = nullptr, const std::function<void()>& P1 = nullptr, const std::function<void()>& P0 = nullptr);
    ~GRAFCETStep();
    void changePeriod(int period_ms);

protected:
    void onEntry(QEvent *event) ;
    void onExit(QEvent *event) ;

private:
    std::mutex mtx;
    QTimer *timer_step;            //Cyclic timer of execution of the function
    std::function<void()> N;        //Function to be executed cyclically
    std::function<void()> P1;       //Function to be executed at start step
    std::function<void()> P0;       //Function to be executed at end step
};
#endif // GRAFCETSTEP_H