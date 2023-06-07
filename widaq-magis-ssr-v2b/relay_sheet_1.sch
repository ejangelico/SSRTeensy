EESchema Schematic File Version 4
LIBS:widaq-magis-ssr-v1b-cache
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 2 3
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L SN74AHCT125PW:SN74AHCT125PW BUF_0-3
U 1 1 615CBA17
P 4350 1950
AR Path="/615C3C31/615CBA17" Ref="BUF_0-3"  Part="1" 
AR Path="/61701D11/615CBA17" Ref="BUF_0-?"  Part="1" 
AR Path="/61705328/615CBA17" Ref="BUF_8-11"  Part="1" 
F 0 "BUF_0-3" H 4350 2820 50  0000 C CNN
F 1 "SN74AHCT125N" H 4350 2729 50  0000 C CNN
F 2 "Housings_DIP:DIP-14_W7.62mm" H 4350 1950 50  0001 L BNN
F 3 "" H 4350 1950 50  0001 L BNN
	1    4350 1950
	1    0    0    -1  
$EndComp
Text HLabel 3450 1550 0    50   Input ~ 0
ch0
Text HLabel 3450 1650 0    50   Input ~ 0
ch1
Text HLabel 3450 1750 0    50   Input ~ 0
ch2
Text HLabel 3450 1850 0    50   Input ~ 0
ch3
Text GLabel 5050 2550 2    50   Input ~ 0
dgnd
Text GLabel 5050 1350 2    50   Input ~ 0
5V
Wire Wire Line
	3650 1650 3450 1650
Wire Wire Line
	3450 1750 3650 1750
Wire Wire Line
	3650 1850 3450 1850
$Comp
L SN74AHCT125PW:SN74AHCT125PW BUF_4-7
U 1 1 616DD52A
P 4350 4600
AR Path="/615C3C31/616DD52A" Ref="BUF_4-7"  Part="1" 
AR Path="/61701D11/616DD52A" Ref="BUF_4-?"  Part="1" 
AR Path="/61705328/616DD52A" Ref="BUF_12-15"  Part="1" 
F 0 "BUF_4-7" H 4350 5470 50  0000 C CNN
F 1 "SN74AHCT125N" H 4350 5379 50  0000 C CNN
F 2 "Housings_DIP:DIP-14_W7.62mm" H 4350 4600 50  0001 L BNN
F 3 "" H 4350 4600 50  0001 L BNN
	1    4350 4600
	1    0    0    -1  
$EndComp
Text HLabel 3450 4200 0    50   Input ~ 0
ch4
Text HLabel 3450 4300 0    50   Input ~ 0
ch5
Text HLabel 3450 4400 0    50   Input ~ 0
ch6
Text HLabel 3450 4500 0    50   Input ~ 0
ch7
Text GLabel 5050 5200 2    50   Input ~ 0
dgnd
Text GLabel 5050 4000 2    50   Input ~ 0
5V
Wire Wire Line
	3450 4200 3650 4200
Wire Wire Line
	3650 4300 3450 4300
Wire Wire Line
	3450 4400 3650 4400
Wire Wire Line
	3650 4500 3450 4500
Text GLabel 3300 2350 0    50   Input ~ 0
dgnd
Wire Wire Line
	3650 1950 3400 1950
Wire Wire Line
	3400 1950 3400 2050
Wire Wire Line
	3400 2350 3300 2350
Wire Wire Line
	3650 2250 3400 2250
Connection ~ 3400 2250
Wire Wire Line
	3400 2250 3400 2350
Wire Wire Line
	3650 2150 3400 2150
Connection ~ 3400 2150
Wire Wire Line
	3400 2150 3400 2250
Wire Wire Line
	3650 2050 3400 2050
Connection ~ 3400 2050
Wire Wire Line
	3400 2050 3400 2150
Text GLabel 3300 5000 0    50   Input ~ 0
dgnd
Wire Wire Line
	3650 4600 3400 4600
Wire Wire Line
	3400 4600 3400 4700
Wire Wire Line
	3400 5000 3300 5000
Wire Wire Line
	3650 4900 3400 4900
Connection ~ 3400 4900
Wire Wire Line
	3400 4900 3400 5000
Wire Wire Line
	3650 4800 3400 4800
Connection ~ 3400 4800
Wire Wire Line
	3400 4800 3400 4900
Wire Wire Line
	3650 4700 3400 4700
Connection ~ 3400 4700
Wire Wire Line
	3400 4700 3400 4800
Wire Wire Line
	3450 1550 3650 1550
$Comp
L Connector:Screw_Terminal_02x08 T1
U 1 1 61F8A47A
P 7950 2700
AR Path="/615C3C31/61F8A47A" Ref="T1"  Part="1" 
AR Path="/61705328/61F8A47A" Ref="T2"  Part="1" 
F 0 "T1" V 7375 2725 50  0000 C CNN
F 1 "Screw_Terminal_02x08" V 7466 2725 50  0000 C CNN
F 2 "teensy:terminalblock_02x08_offset" H 7350 2700 50  0001 C CNN
F 3 "" H 7350 2700 50  0001 C CNN
	1    7950 2700
	0    1    1    0   
$EndComp
Wire Wire Line
	5050 1550 7350 1550
Wire Wire Line
	5050 1650 7250 1650
Wire Wire Line
	5050 1750 7150 1750
Wire Wire Line
	7150 1750 7150 2550
Wire Wire Line
	7150 2550 7650 2550
Wire Wire Line
	5050 1850 7050 1850
Wire Wire Line
	7050 1850 7050 2650
Wire Wire Line
	7050 2650 7650 2650
Wire Wire Line
	5050 4200 7050 4200
Wire Wire Line
	5050 4300 7150 4300
Wire Wire Line
	5050 4400 7250 4400
Wire Wire Line
	7250 4400 7250 2950
Wire Wire Line
	7250 2950 7650 2950
Wire Wire Line
	5050 4500 7350 4500
Wire Wire Line
	7350 4500 7350 3050
Wire Wire Line
	7350 3050 7650 3050
Text GLabel 8300 3650 2    50   Input ~ 0
dgnd
Wire Wire Line
	8300 2350 8300 2450
Connection ~ 8300 2450
Wire Wire Line
	8300 2450 8300 2550
Connection ~ 8300 2550
Wire Wire Line
	8300 2550 8300 2650
Connection ~ 8300 2650
Wire Wire Line
	8300 2650 8300 2750
Connection ~ 8300 2750
Wire Wire Line
	8300 2750 8300 2850
Connection ~ 8300 2850
Wire Wire Line
	8300 2850 8300 2950
Connection ~ 8300 2950
Wire Wire Line
	8300 2950 8300 3050
Connection ~ 8300 3050
Wire Wire Line
	8300 3050 8300 3650
Wire Wire Line
	7050 4200 7050 2850
Wire Wire Line
	7050 2850 7650 2850
Wire Wire Line
	7150 4300 7150 2750
Wire Wire Line
	7150 2750 7650 2750
Wire Wire Line
	7250 1650 7250 2350
Wire Wire Line
	7250 2350 7650 2350
Wire Wire Line
	7350 1550 7350 2450
Wire Wire Line
	7350 2450 7650 2450
$EndSCHEMATC
