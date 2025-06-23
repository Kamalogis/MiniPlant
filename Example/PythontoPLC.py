from pymodbus.client import ModbusTcpClient
from time import sleep

client = ModbusTcpClient('10.10.17.210')
sleep(2)

print("Connect to PLC.....")
if client.connect():
    print("Done connecting")
else :
    print("Gagal cuk")

count = 0
while True:
    #Kirim data ke Memory Word
    print(count)
    client.write_register(0, count)
    client.write_register(2, count)
    client.write_register(4, count)
    client.write_register(6, count)
    client.write_register(8, count)
    client.write_register(10, count)
    client.write_register(12, count)
    client.write_register(14, count)
    count += 100
    # sleep(0.1)
    if count > 1000:
        count = 0

client.close()
# video.release()
cv.destroyAllWindows()