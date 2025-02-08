# README #

### What is this repository for? ###

* An impedance analyzer using a 'shield' for a STM32 Nucleo-G474 devboard
* DAC 1 is used to generate a high frequency sinusoid via DMA
* ADC 1 and ADC 2 are operated in 'dual mode' to measure the voltage applied to the DUT, and the current through a sense resistor
* the voltage and current measurement are used to calculate the impedance at this given frequency
* this is repeated many times to perform an impedance sweep and output a bode plot on the Python windows app included

### Setup ###

* clone this repo to your local WSL directory, e.g. `\\wsl.localhost\Ubuntu\home\clalumiere\Jobs\personal\ImpedanceAnalyzer`
* CD into the newly created directory
* open the directory with VScode `code . `
* In the bottom-right, VS code will prompt you to reopen in devcontainer
* The devcontainer will take considerable time to generate the first time
* select the **[Debug]** build option in the bottom toolbar (wrench + screwdriver icon)
* compile using the 'Build' button in the bottom toolbar (gear icon)
* launch Ozone and connect a J-link debugger to your microcontroller
* open `Nucleo.jdebug` and flash the microcontroller

### Compiling the protobufs ###
* in a terminal (including the terminal inside VS code), type the following
```bash
cd ./messages
./build.sh
```

### Running the Python PC com tool ###
* install Python 3 - [Link](https://www.python.org/ftp/python/3.12.4/python-3.12.4-amd64.exe)
* clone this repo to a local directory in Windows
* open a **command prompt** window in Windows (not powershell)
* CD to your newly cloned repo
```commandline
python -m venv .venv
.venv\Scripts\activate.bat pip install -r requirements.txt
```
* run pc_com.bat

### Updating Python Virtual Environment ###
If additional packages are added to this project, update requirements.txt with 

```commandline
pip freeze > requirements.txt
```

### Updating the GUI ###

The GUI is developed using the QT framework. Open `designer.exe` located in `.venv\Lib\site-packages\PySide6`

The design outputs a `main_window.ui` file, which needs to be converted to python. From the command line, call:

```commandline
GUIRebuild.bat
```



# Contacts

| Person        | Role               | Email                  |
|---------------|--------------------|------------------------|
| Craig Lalumiere | Project Lead       | cjlalumi@uwaterloo.ca |
