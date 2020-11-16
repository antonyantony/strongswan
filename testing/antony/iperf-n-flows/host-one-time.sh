python /home/secunet/spirent_PerfInterfaces_innova.py IPv4
for i in $(seq 18 71); do echo 0 > /sys/devices/system/cpu/cpu${i}/online; done
