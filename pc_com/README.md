# Introduction

This tool provides a PC communication interface for Portage system.

# Installation

## Dependencies (Windows Installation)
1. Python 3 - [Link](https://www.python.org/ftp/python/3.12.4/python-3.12.4-amd64.exe)

## Setup
Clone to repo to a folder (on Windows)

```commandline
git clone git@bitbucket.org:inertiapd/portage.git
```

Create a virtual environment by opening the project folder with a command terminal.

NOTE: scripts use BATCH files, **do not use powershell!**

```commandline
python -m venv .venv
.venv\Scripts\activate.bat pip install -r requirements.txt
```

## Running

From command terminal:

```commandline
pc_com.bat
```

# Updating Python Virtual Environment

If additional packages are added to this project, update requirements.txt with 

```commandline
pip freeze > requirements.txt
```

# Updating the GUI

The GUI is developed using the QT framework. Open `designer.exe` located in `.venv\Lib\site-packages\PySide6`

The designed outputs a `main_window.ui` file, which needs to be converted to python. From the command line, call:

```commandline
GUIRebuild.bat
```



# Contacts

| Person        | Role               | Email                  |
|---------------|--------------------|------------------------|
| Mitch English | Project Lead       | menglish@inertiapd.com |
