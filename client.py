import socket
import random
from threading import Thread
from datetime import datetime
from colorama import Fore, init

#init colors
init()
#set the available colors
colors = [Fore.BLUE, Fore.CYAN, Fore.GREEN, Fore.RED, Fore.WHITE, Fore.YELLOW, 
    Fore.LIGHTBLUE_EX, Fore.LIGHTCYAN_EX, Fore.LIGHTGREEN_EX, Fore.LIGHTRED_EX]
#set random color for the client
client_color = random.choice(colors)

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 9032

#separates the client name and message
separator_token = " --> "
sending_token = " sent file:"

#initialize TCP socket
s = socket.socket()
print(f"[*] Connecting to {SERVER_HOST}:{SERVER_PORT}...")
#connect to the server
s.connect((SERVER_HOST, SERVER_PORT))
print("[+] Connected.")
#prompt the client for a name
username = input("Enter your name: ")

def listen_for_messages():
    while True:
        message = s.recv(4096).decode()
        #f = open("check.txt", "a")
        #f.write(message)
        #f.close()
        print(message)

#make a thread that listens for messages to this client & print them
t = Thread(target = listen_for_messages)
#make the thread daemon so it ends whenever the main thread ends
t.daemon = True
#start the thread
t.start()

while True:
        #input message to send to the server
        to_send = input()
        #exit the program
        if to_send.lower() == 'q':
            break
        elif to_send[0] == '/':
            #add the datetime, name and the color of the sender
            date_now = datetime.now().strftime('%H:%M:%S')
            #send info about sender
            s.send((f"{client_color}[{date_now}] {username}{sending_token}{Fore.RESET}").encode())
            #send the message
            s.send(to_send.encode())
        else:
            date_now = datetime.now().strftime('%H:%M:%S') 
            to_send = f"{client_color}[{date_now}] {username}{separator_token}{to_send}{Fore.RESET}"            
            s.send(to_send.encode())

#close the socket
s.close()
