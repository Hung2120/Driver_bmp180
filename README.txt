BMP180 Raspberry Pi 4
THANH VIEN NHOM:
NGUYEN CONG HUNG        22146133
NGUYEN TUAN KIET        22146161
HUYNH LUU NHAT THUAN    22146237
===========================================================================
📌 Ứng dụng thực tế
Cảm biến BMP180 được sử dụng rộng rãi trong các hệ thống nhúng để:
Đo áp suất không khí và nhiệt độ môi trường.
Tính toán độ cao (altitude) — ứng dụng trong drone, thiết bị đo độ cao, và hệ thống định vị GPS nâng cao.
Ứng dụng trong trạm thời tiết mini, thiết bị IoT và các hệ thống điều khiển môi trường.
===========================================================================
Cau truc du an
bmp180_project/
├── bmp180_overlay.dtbo                  
├── Makefile                    
├── bmp180_driver.c                          
├── bmp180_user.c  
===========================================================================
Cac buoc chay du an
-----------------------------------------------------------
Buoc 1: Enable I2C on Raspberry Pi
-----------------------------------------------------------
-Chay:
  sudo raspi-config
-Vao muc:
  Interface Options → I2C → Enable
-Reboot Pi cua ban sau khi enable I2C thanh cong.
-----------------------------------------------------------
STEP 2: Build the Kernel Driver
-----------------------------------------------------------
-Trong thu muc du an chay lenh 
      make
-Kiem tra thiet bi nao o dia chi 0x77 khong (vi du bmp280) 
      dmesg | grep -i 'i2c\|bmp'
[    8.589546] bmp280 1-0077: supply vddd not found, using dummy regulator
[    8.589856] bmp280 1-0077: supply vdda not found, using dummy regulator
[    8.685570] bmp280: probe of 1-0077 failed with error -121
-> Driver bmp280 co san trong he dieu hanh pi 4 dang chiem dia chi cua driver du an can unblind de bmp180 blind bang lenh 
      echo 1-0077 | sudo tee /sys/bus/i2c/drivers/bmp280/unbind
-Load module
      sudo insmod ./bmp180_driver.ko
-Check log
      dmesg | grep bmp180
-----------------------------------------------------------
STEP 3: Compile User-Space Application
-----------------------------------------------------------
      gcc bmp180_user.c -o run
-----------------------------------------------------------
STEP 4: Use Device Tree Overlay (Optional but Recommended)
-----------------------------------------------------------
-Bien dich overlay
      sudo dtc -@ -I dts -O dtb bmp180_overlay.dts -o bmp180_overlay.dtbo
-Di chuyen file .dtbo vao thu muc overlays
      sudo cp bmp180_overlay.dtbo /boot/overlays/
-Chinh sua file cau hinh boot
      sudo nano /boot/config.txt
-Them dong sau vao cuoi file
      dtoverlay=bmp180_overlay
-Khoi dong lai thiet bi
      sudo reboot
-----------------------------------------------------------
STEP 5: Run the User-Space Test
-----------------------------------------------------------
-Chay lai cac code sau
      1) echo 1-0077 | sudo tee /sys/bus/i2c/drivers/bmp280/unbind
      2) sudo insmod ./bmp180_driver.ko
      3) sudo ./run
-Ket qua chay mong muon
Temperature: 30.2 °C
Pressure: 1009.97 hPa
->Nhiet do thuc te va ap suat khi quyen tuy tung khu vuc
-----------------------------------------------------------
To Clean:
-----------------------------------------------------------
      make clean
-----------------------------------------------------------
To Remove Module:
-----------------------------------------------------------
      sudo rmmod bmp180_driver
-----------------------------------------------------------
Notes:
-----------------------------------------------------------
    -Dam bao cam bien bmp180 da duoc ket noi voi I2C1 (mac dinh la chan SDA1/SCL1 tren Raspberry Pi).
    -Dia chi I2C mac dinh cua bmp180 la 0x77.
    -Neu ban dang su dung nhieu thiet bi I2C, hay chac chan khong co xung dot dia chi(dam bao loai bo module bmp280 truoc khi chay chhuong trinh)
