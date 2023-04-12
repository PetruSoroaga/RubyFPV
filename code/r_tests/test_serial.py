import RPi.GPIO as GPIO
import time
import serial
import array

readBuffer = array.array('c')
data_output=[]

ser = serial.Serial( port="/dev/ttyUSB0", baudrate=57600, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, bytesize=serial.EIGHTBITS)

for x in range(32):
	print hex(ord(ser.read(1)))
ser.close()

