#include "GRAFCETExample.h"

GRAFCETExample::GRAFCETExample()
{
    //Crear los estados
    GRAFCETStep *s1 = new GRAFCETStep("s1", 500, std::bind(&GRAFCETExample::func_s1, this), std::bind(&GRAFCETExample::entry_s1, this));
    GRAFCETStep *s2 = new GRAFCETStep("s2", 500, std::bind(&GRAFCETExample::func_s2, this), nullptr, std::bind(&GRAFCETExample::exit_s2, this));
    GRAFCETStep *s3 = new GRAFCETStep("s3", 500, std::bind(&GRAFCETExample::func_s3, this));

    //enlaza las transiciones
    s1->addTransition(this, SIGNAL(goToS2()), s2);
    s2->addTransition(this, SIGNAL(goToS3()), s3);
    s2->addTransition(this, SIGNAL(goToS1()), s1);
    s3->addTransition(this, SIGNAL(goToS1()), s1);

    // Configuración de la Máquina de Estado
    machine.addState(s1);
    machine.addState(s2);
    machine.addState(s3);

    machine.setChildMode(QState::ExclusiveStates);  // Maquina en serie sin estados paralelos
    machine.setInitialState(s1);                    // Estado inicial

    //Arranca la maquina
    machine.start();
    auto error = machine.errorString();
    if (error.length() > 0){
        qWarning() << error;
        throw error;
    }


}

//Función de visualización de estados activos
void GRAFCETExample::transition(){
    QSet<QAbstractState*> activeStates = machine.configuration();
    for(QAbstractState* state : activeStates) 
        qDebug() << "Estado activo:" << state->objectName();
}



/////////////////////Funciones de ejecución de estados////////////////////

/////////////////////Función de entrada a estado////////////////////////////

void GRAFCETExample::entry_s1(){
    qInfo() << "///////Entrando al estado s1 desde la función de entrada///////////////////";
    transition();
}

///////////////////Función de salida a estado////////////////////////////
void GRAFCETExample::exit_s2(){
    qInfo() << "///////Salida del estado s2 desde la función de salida///////////////////";
}
///////////////////////////Como si fueran computes///////////////////////
void GRAFCETExample::func_s1() {
    
    qInfo()<<"s1"<<i;
    i++;

    if (i>5){
        i = 0;
        emit goToS2();
    }    
}
void GRAFCETExample::func_s2() {
    if (check)
    {
        qInfo()<<"s2"<<n;
        n--;
    }
    else{
        qInfo()<<"s2"<<n;
        n++;
    }
    
    if (n>5){
        n = 0;
        check=true;
        emit goToS3();
    }
    if (n<-5){
        n = 0;
        emit goToS1();
    }
}
void GRAFCETExample::func_s3() {
    qInfo()<<"s3"<<j;
    j++;

    if (j>10){
        j = 0;
        emit goToS1();
    }
}
