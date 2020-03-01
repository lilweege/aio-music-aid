#! /usr/bin/env python
#  -*- coding: utf-8 -*-


'''
Description: LUQ MetroTuner Pro - All-In-One music aid/practice tool: Python gui code with tkinter on rpi
Course Code: TER4M
Submitted by: Luigi Quattrociocchi & Umberto Quattrociocchi
Submitted to: Mr. Kolch
Date Submitted: January 19th, 2020
'''

# this python file is a support to share common functions not necessesarily specific to our gui
# this file is mainly responsible for sending serial outputs and toggling/changing values on button presses


# import necessary libraries
import sys
import os
import serial
from serial import SerialException

# check if arduino is currently present in devices under pre-determined port
try:
    ser = serial.Serial('/dev/ttyACM0', 9600)
except SerialException:
    ser = None

# check python version
try:
    import Tkinter as tk
except ImportError:
    import tkinter as tk

try:
    import ttk
    py3 = False
except ImportError:
    import tkinter.ttk as ttk
    py3 = True

# declare and init global variables shared between support and main (tk type variables)
def set_Tk_var():
    global metronome, tuner, drone, beats, sharps, bpm, hz, octave, note, wave
    bpm = tk.StringVar()
    hz = tk.StringVar()
    octave = tk.StringVar()
    note = tk.StringVar()
    bpm.set('90')
    hz.set('440')
    octave.set('4')
    note = None
    wave = 'Sine'
    metronome = False
    beats = 0
    tuner = False
    drone = False
    sharps = True
    

# tkinter generated function, creates a toplevel gui window
def init(top, gui, *args, **kwargs):
    global w, top_level, root
    w = gui
    top_level = top
    root = top

# tkinter generated function, destroyes toplevel gui window
def destroy_window():
    global top_level
    top_level.destroy()
    top_level = None

# if this file music_aid_support.py is being run directly or being accessed by the other file
# by default, the other file is run, so this will not be called.
if __name__ == '__main__':
    import music_aid
    music_aid.vp_start_gui()



# general purpose serial output write
def send(s):
    # ser is the serial connection to arduino device
    # encode changes string to char and byte format readable by arduino
    ser.write(s.encode())


'''
All 'submit' and 'toggle' prefix named functions are called from the other file as a command of a gui input.
The function called will change the affected global variable and then use the above send function to send to the arduino.
The sent string has two parts, a letter code for the arduino to determine which value to change, and then the value.
The arduino will recieve the string and parse it for the code and value.
All functions are short and handle their own changes only.
'''

def submitBeats(n):
    send('B' + str(n))

def submitBPM(n):
    bpm.set(str(n))
    send('B' + bpm.get())

def sumbitHZ(n):
    hz.set(str(n))
    send('H' + hz.get())

def sumbitOctave(n):
    octave.set(str(n))
    send('O' + octave.get())
    
def sumbitNote(n):
    note = str(n)
    send('N' + note)

def sumbitWave(n):
    wave = str(n)
    send('W' + wave)

def sumbitMV(n):
    send('M' + str(int(n)))

def sumbitDV(n):
    send('D' + str(int(n)))

def tunerSharps(n):
    send('T' + ('FLATS','SHARPS')[int(n) == 1])
    
def droneSharps(n):
    send('D' + ('FLATS','SHARPS')[int(n) == 1])

def toggleMetronome():
    global metronome
    metronome = not metronome
    send('M' + ('OFF','ON')[metronome])
    return metronome

def toggleTuner():
    global tuner
    tuner = not tuner
    send('T' + ('OFF','ON')[tuner])
    return tuner

def toggleDrone():
    global drone
    drone = not drone
    send('D' + ('OFF','ON')[drone])
    return drone
