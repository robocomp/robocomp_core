/****************************************************************************
* File name: GRAFCETStep.cpp
* Author: Alejandro Torrejon Harto
* Date: 05/10/2023
* Description: QState wrapper to execute functions, simulating a GRAFCET or SFC(Sequential Function Chart) step (according to EN61131-3).
****************************************************************************/

#include "GRAFCETStep.h"

/**
 * @brief Builds GRAFCETStep object
 *
 * This function takes the name of the step, execution period, function to execute cyclically, function in case of entry and exit, to build the object.
 *
 * @param name Name of the step.
 * @param period_ms Period of cyclic execution.
 * @param N Function to execute cyclically when the step is active.
 * @param P1 Function to execute when entering the step
 * @param P0 Function to execute when exiting the step
 */
GRAFCETStep::GRAFCETStep(QString name, int period_ms, const std::function<void()>& N, const std::function<void()>& P1, const std::function<void()>& P0){
    #if DEBUG
        std::cout << "Construyendo GRAFCETStep "<< name.toStdString() <<std::endl<<std::flush;
    #endif
    this->setObjectName(name);                  // Set object name

    this->N = N;                                // Set function to execute cyclically
    this->P1 = P1;                              //Function to be executed at start step
    this->P0 = P0;                              //Function to be executed at end step

    if (this->N != nullptr)
    {
        this->timer_step = new QTimer(this);       //Create cyclic timer
        this->timer_step->setInterval(period_ms);  //Set time to cyclic timer
        //Connecting the cyclic timer to the function 
        connect(this->timer_step, &QTimer::timeout, this, [this](){
            if (fps_timer_.isValid()) {
                const float dt_ms = static_cast<float>(fps_timer_.elapsed());
                if (dt_ms > 0.f) {
                    const float inst_fps = 1000.f / dt_ms;
                    actual_fps_ = (actual_fps_ == 0.f) ? inst_fps
                                                       : 0.9f * actual_fps_ + 0.1f * inst_fps;
                }
            }
            fps_timer_.restart();
            this->N();
        });
    }
    #if DEBUG
        std::cout << "Construido GRAFCETStep"<< this->objectName().toStdString() <<std::endl<<std::flush;
    #endif
}

/**
 * @brief Destroy GRAFCETStep object
 *
 * This function destroys and frees the object's memory
 */
GRAFCETStep::~GRAFCETStep() 
{
    if (this->N != nullptr)
    {
        this->timer_step->stop();  //Stop cyclic timer
        delete this->timer_step;   //Delete cyclic timer
    }
    #if DEBUG
        std::cout << "Dell GRAFCETStep"<< this->objectName().toStdString()<<std::endl<<std::flush;
    #endif
}

/**
 * @brief Changes the period of the cyclic execution
 *
 * This function modifies the execution period and restarts with the new time in case it is active.
 *
 * @param period_ms Period of cyclic execution.
 */
void GRAFCETStep::setPeriod(int period_ms)
{
    if (this->N != nullptr)
    {
        //Set new time to cyclic timer
        this->timer_step->setInterval(period_ms);
        this->mtx.lock();
        //Restart cyclic timer
        if (this->timer_step->isActive())
        {
            timer_step->stop();
            timer_step->start();
        }
        this->mtx.unlock();
    }
}

/**
 * @brief Get the period of the cyclic execution
 *
 * This function return the execution period.
 *
 * @return Cyclic execution period otherwise -1.
 */
int GRAFCETStep::getPeriod()
{
    if (this->N != nullptr)
        return this->timer_step->interval();
    else
        return -1;
}

/**
 * @brief Return the measured actual FPS of the cyclic N function.
 *
 * Uses an EMA (α=0.9) over inter-call intervals. Returns 0 before the first call.
 */
float GRAFCETStep::getActualFps() const
{
    return actual_fps_;
}

/**
 * @brief When the step is activated this function is executed. 
 *
 * This function launches the cyclic timer that executes the configured function, event is not used.
 */
void GRAFCETStep::onEntry(QEvent *event)
{
    Q_UNUSED(event)

    if (this->P1 != nullptr)
        this->P1();                     //Launches the entry function

    if (this->N != nullptr)
        this->timer_step->start();     //Launches the cyclic timer
    #if DEBUG
        std::cout << "Entrando en GRAFCETStep "<< this->objectName().toStdString() <<std::endl<<std::flush;
    #endif
}

/**
 * @brief When the step is deactivated this function is executed. 
 *
 * This function stops the cyclic timer that executes the configured function, event is not used.
 */
void GRAFCETStep::onExit(QEvent *event)
{
    Q_UNUSED(event)
    if (this->N != nullptr){
        this->mtx.lock();
        this->timer_step->stop();      //Stops the cyclic timer
        this->mtx.unlock();
    }
    if (this->P0 != nullptr)
        this->P0();                    //Launches the exit function

    #if DEBUG
        std::cout<< "Saliendo de GRAFCETStep "<< this->objectName().toStdString() <<std::endl<<std::flush;
    #endif
}