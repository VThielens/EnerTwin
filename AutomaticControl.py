import pyautogui as pya
import serial as sr
import datetime
import time
import numpy as np
import pandas as pd
import pyperclip
import os
import csv

os.chdir(r"C:\Users\MTT\Desktop\TestTHIELENS\GitHub")

# open the excel with testing set
TESTING_FILE = r"testing_campaign.xlsx" # to change

# open the excel with testing set
SAVE_FILE = "test_file_"+time.strftime("%Y_%m_%d_%H%M%S")+".csv" # to change

def indentify_location():
    """Allow to indentify the location of the mouse at the begining
    """
    for i in range(10):
        print(pya.position())
        print("\n")
        time.sleep(1)

def enter_SP(SP: int, position_x:int, position_y: int):#OK TESTED
    # we move the cursor to the right location
    pya.moveTo(position_x, position_y)
    # we click at that point
    pya.click(clicks=2)
    pya.write("{}".format(int(SP)), interval = 0.25)
    pya.press('enter')

def write_arduino(temp:float, inner_close:float, extern_close: float, back_pressure:float, arduino: sr.Serial): 
    to_write = f"{temp},{inner_close},{extern_close},{back_pressure}\n"
    arduino.write(bytes(to_write, 'utf-8')) 
    time.sleep(1)

def read_arduino(arduino: sr.Serial):
    data = arduino.readline() 
    return data

def check_SP(SP_given, position_x, position_y):
    # we move the cursor to the right location
    pya.moveTo(position_x, position_y)
    # we click at that point
    pya.click(clicks=2)
    # we copy the text
    pya.hotkey('ctrl', 'c')
    # let delay
    time.sleep(0.1)
    # take the copied set point
    SP_copied = float(pyperclip.paste().replace(',','.'))
    # we need to round it
    SP_rounded = np.round(SP_copied,1)
    if SP_rounded != SP_given: # si différent on retourne FALSE
        return False
    else:
        return True # si identique, on retourne TRUE

def checking_function(seconds_to_wait: float, n_check: int, x_read, y_read):
    # size of the vector
    seconds_between_check = seconds_to_wait/n_check
    for i in range(n_check):
        time.sleep(seconds_between_check)
        check_result = check_SP(row['SP'], x_read, y_read) # check if SP != value it has been given
        if check_result == False:
            return False
    return True

if __name__ == '__main__':
    #########################
    ## TO FILL ####
    #########################
    # time interval to check if turbine is stopped
    seconds_between_check = 10*60
    # position to enter SP
    x_click = 1846 # to change
    y_click = 136 # to change
    # position to read SP
    x_read = 1786 # to change
    y_read = 137 # to change
    # position to clear
    x_reset = 787 # to change 
    y_reset = 831 # to change
    with open(SAVE_FILE, "w", newline="") as f:
        writer = csv.writer(f, delimiter=",")
        writer.writerow(['temp','inner_close','extern_close','back_pressure','SP', 'time','comment'])
        # prepare a saving logbook
        logbook = []
        # get time to move to the turbine sheet and click on it
        time.sleep(5)
        # save start time
        start_time = time.localtime()
        # we create the Arduino communication
        arduino = sr.Serial(port='COM5', baudrate=9600, timeout=.1) 
        
        # we read the testing sheet
        df_test = pd.read_excel(TESTING_FILE)
        # create a Boolean if we can apply the state
        applyState = True
        # create a parameter when we want to skip row that does not provide 0% EGR
        goToNoEGR = False
        # we loop through the testing sheet
        for index, row in df_test.iterrows():
            if goToNoEGR: # if I should reach a state without EGR bcs of a flameout
                # check if the state has no EGR
                IsNoEGR = (row['inner_close'] == 1 and row['extern_close']==1 and row['back_pressure']==0)
                if IsNoEGR: # je n'ai pas d'EGR dans mon state --> ok je peux l'appliquer et je ne chercherai plus à atteindre 0 EGR et je rajoute du temps pour stabilisation
                    applyState = True
                    goToNoEGR = False
                    row['delay_before_next']+=60 # on rajoute 60 minutes pour que ça redémarre + stabilisation
                else: # j'ai de l'EGR, je continue la recherche et je n'applique pas cet état
                    applyState = False
                    goToNoEGR = True
            if applyState:
                # we enter the setpoint
                enter_SP(row['SP'], x_click, y_click)
                # we write the closing value to Arduino
                write_arduino(row['temperature'], row['inner_close'], row['extern_close'], row['back_pressure'], arduino)
                # we save the action in the logbook
                to_save = {'temp': row['temperature'],
                        'inner_close': row['inner_close'],
                        'extern_close': row['extern_close'],
                        'back_pressure': row['back_pressure'],
                        'SP': row['SP'],
                        'time':str(datetime.datetime.now()),
                        'comment':'No'}
                writer.writerow(to_save.values())
                # we print to know the status
                print(to_save)
                # seconds to wait before next set point
                seconds_to_wait = row['delay_before_next']*60
                check_result = checking_function(seconds_to_wait, 5, x_read, y_read)
                if check_result == False:
                    goToNoEGR = True
                    to_save['time'] =str(datetime.datetime.now())
                    to_save['comment'] = 'Error'
                    writer.writerow(to_save.values())
                    # we move the cursor to the reset location
                    pya.moveTo(x_reset, y_reset)
                    # we click at that point
                    pya.click(clicks=1)
    # end
    arduino.close()

