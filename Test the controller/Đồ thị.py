import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

file_path = r'D:\NCKH\Thử nghiệm bộ điều khiển\Bộ điều khiển 5.csv'  
angleData = pd.read_csv(file_path, header=None)  

angleData = pd.to_numeric(angleData.iloc[:, 0], errors='coerce')  


angleData = angleData.dropna()  

angleData = angleData.values  

num_points = int(len(angleData))  
time = np.linspace(0, 3000, num_points)  


plt.figure(figsize=(12, 6))  
plt.plot(time, angleData, color='b', label='Góc thực tế')  

plt.ylim(-80, 80)  

plt.axhline(y=-4, color='g', linewidth=1, label='Góc cân bằng')  


plt.title('Biểu đồ Góc Thực Tế 5')
plt.xlabel('Thời gian (ms)')
plt.ylabel('Góc (độ)')  
plt.grid(True) 

plt.legend(loc='best')

plt.show()
