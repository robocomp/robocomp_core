# GRAFCETStep

GRAFCETStep is a QState wrapper to execute functions cyclically while the state/step is active.

Simulates a GRAFCET or SFC-Sequential Function Chart step (according to EN61131-3)

The functions used can be static or bound to an object to use its own parameters and functions.

# Dependencies
```bash
    sudo apt install qt6-scxml-dev libqt6statemachineqml6 libqt6statemachine6
```

## Constructor
```c++
    explicit GRAFCETStep(QString name, int period_ms = 100, const std::function<void()>& N = nullptr, const std::function<void()>& P1 = nullptr, const std::function<void()>& P0 = nullptr);
```

where:
- name is the name of the step
- period_ms is the period of execution of the function associated with the step.
- N is the function that will be executed cyclically within the step, it shall return void and have no parameters.
- P1 is the function that will be executed when entering the step, it shall return void and have no parameters.
- P0 is the function that will be executed when exiting the step, it shall return void and have no parameters.

## Example of construction with a static function
Will cyclically execute the function, but will have no communication with the "outside".

```c++
    GRAFCETStep *s1 = new GRAFCETStep("s1", 500, function);
    GRAFCETStep *s2 = new GRAFCETStep("s2", 500, nullptr, nullptr, exit_function);
```

## Example of construction inside an object
Will cyclically execute the object's function, and will have all parameters and functions of the object

```c++
    GRAFCETStep *s1 = new GRAFCETStep("s1", 500, std::bind(&GRAFCETExample::func_s1, this), std::bind(&GRAFCETExample::entry_s1, this));
    GRAFCETStep *s2 = new GRAFCETStep("s2", 500, std::bind(&GRAFCETExample::func_s2, this), nullptr, std::bind(&GRAFCETExample::exit_s2, this));
```
    

    
