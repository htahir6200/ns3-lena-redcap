#!/usr/bin/python3
import re
import matplotlib.pyplot as plt 

heliostaten = []
drones= []
other= []
x = []
y = []
plt.axis([-600, 600, -600, 600])
file1 = open("position.txt", "r")
for line in file1:
    line = line.split(":")
    if line[0].find("Heliostat") != -1:
        heliostaten.append(line[1])
    elif line[0].find("Drone") !=-1:
        drones.append(line[1])
        
    elif line[0].find("Other") !=-1:
        other.append(line[1])
        
fig, ax = plt.subplots()
for i in heliostaten:
    i = re.findall("[-+]?[.]?[\d]+(?:,\d\d\d)*[\.]?\d*(?:[eE][-+]?\d+)?", i)
    #i = numbers.findall(i)
    x.append(float(i[0])) 
    y.append(float(i[1])) 
plt.plot(x,y,'o',color='b',label = "Heliostaten")

x = []
y = []
for i in drones:
    i = re.findall("[-+]?[.]?[\d]+(?:,\d\d\d)*[\.]?\d*(?:[eE][-+]?\d+)?", i)
    #i = numbers.findall(i)
    x.append(float(i[0])) 
    y.append(float(i[1])) 
plt.plot(x,y,'o',color='r',label = "Drohnen")

x = []
y = []
for i in other:
    i = re.findall("[-+]?[.]?[\d]+(?:,\d\d\d)*[\.]?\d*(?:[eE][-+]?\d+)?", i)
    x.append(float(i[0])) 
    y.append(float(i[1])) 
plt.plot(x,y,'o',color='g',label = "Other")
plt.legend(loc="upper left")
# circle1 = plt.Circle((0, 0), 550, color='r')
#ax.add_patch(circle1)
fig.savefig("temp.png")

