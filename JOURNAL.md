---
title: "Discobox"
author: "JestingDart4369"
description: "Button-controlled party box on an Arduino Uno R4 WiFi with LED ring, WS2805 LED strip, piezo buzzer and DFPlayer Mini MP3 playback"
created_at: "2026-07-29"
---

# October 9: Forge compliance and documentation

today im spending some time for th ecomplience for the forge stuff and writing documentation

Note: i havent really photographed my progress so far, so some of the pictures in the older entries are only similar pictures from the same setup. Going forward i will be more compliant and take photos of every step.

![WS2805 LED strip running a colour test](https://raw.githubusercontent.com/JestingDart4369/Discobox/dev/doc/journal/strip-rainbow-test.png)

**Total time spent: 2 hours**

# October 2: Strip bugs, random() and the DFPlayer

the stirp had shifted index and the coulours were lal wrong and the thing was laging like krasy first i thought that somthing was wrong with the cdode whitch is part fo the answer but not al most of the code is right but some parts not also i had a way to high resitor wierd in whitch was bottelneckink the arduino i looked in the code an found out that radom was the problem by giing through and addin a mesurement output to the cli the dfplayer wasnt working also because i conected it wrong i hat rx to rx an tx to tx when it should be rt to tx and tx to rx beginers luk i think in that weak because holiday break started and stuff i worked like 25 h of witch 5 on frieday and after that some to fro eror finding and stuff

![Wiring plan I used for this session](https://raw.githubusercontent.com/JestingDart4369/Discobox/dev/doc/journal/wiring-diagram.png)

**Total time spent: 25 hours**

# September 24: Test bench with the parts

in the bench photo it is the day i got many of the parts the arduino and the led striop are conected in it to the converter and the psu and the arduino to the converter (5v part) also i had the button wired up and started wireung zp the speaker oart i think in that weak i worke for 15 h 10 h in the weak just here and there and 5 h straight for on friday afternoon

![Test bench with the new LED strip, Arduino and parts](https://raw.githubusercontent.com/JestingDart4369/Discobox/dev/doc/journal/strip-test-bench.jpg)

**Total time spent: 15 hours**

# September 23: Buying the strip and reworking the code

i bought the ws 2806 strip because i wanted rgb also a white for a strobe efecz also i chose an strip that was 12 v becaue cheeper and 3m becsause i plan to install t in my 2 bathrom thats why also its ip rated strip i had to rework the code3 firstly i had ai generate the handker for it because i didnt kmow how and the libary fastled i. was using didnt have souport for that i also added oop programing because i learned about it and som things the ked stri  keps with the led ring and so on

![Test bench with the new LED strip, Arduino and parts](https://raw.githubusercontent.com/JestingDart4369/Discobox/dev/doc/journal/strip-test-bench.jpg)

**Total time spent: 15 hours**

# August 14: Buzzer, button gestures and serial console

the buzzer gave me a bit of a hadkae becaus ei couldnt realy play somthng i found a file from the makers that was about playing sound (ardduino docs) but i dont realy knwo wher but transfering a file to that to be ülayed most music has multipple sounds playing at the same time i nedded music whitch only has 1 track playing so it would soudn good so this wax also realy hard  also the gestures it tellls the button presses apart by time betwen presse it tokk me a bit of time to rap my head around that i added a serial consol to be fatsser at debuging and also testing it  i worked maybe 20 h 5h at my grandma 10 h at home and also some time on my school path but idk how long there and when but havent comited every singel change some of the time was also just admin stuff

![Breadboard wiring close-up](https://raw.githubusercontent.com/JestingDart4369/Discobox/dev/doc/journal/breadboard-closeup.jpg)

**Total time spent: 20 hours**

# July 29: Why I started this project

a firend of my gave me the idea why the uno r4 i had it on hand  ther first bread board had a button 12 argb led ring for df robot  took for research i would guess 12 h Buzzer it should be not to loaded with boutons also from the ipods ore headphonesyou already know the gestures i use for pla stop forward skip and backward

![Breadboard wiring close-up of the first prototype](https://raw.githubusercontent.com/JestingDart4369/Discobox/dev/doc/journal/breadboard-closeup.jpg)

**Total time spent: 12 hours**
