from ina219 import INA219, DeviceRangeError
from time import sleep

SHUNT_OHMS = 0.1
MAX_EXPECTED_AMPS = 2.0
ina = INA219(SHUNT_OHMS, MAX_EXPECTED_AMPS)
ina.configure(ina.RANGE_16V)

def read_ina219():
    try:
        print('Bus Voltage: {0:0.2f}V'.format(ina.voltage()))
        print('Bus Current: {0:0.2f}mA'.format(ina.current()))
        print('Power: {0:0.2f}mW'.format(ina.power()))
        print('Shunt Voltage: {0:0.2f}mV\n'.format(ina.shunt_voltage()))
    except DeviceRangeError as e:
        # Current out of device range with specified shunt resister
        print(e)

while 1:
    read_ina219()
    sleep(1)
