# InspRecovery
Recovery application for Buildroot projects

This application will create copies of /mnt/app, /mnt/app/data, and any other relevant file added by insp_LinuxUpdate.txt to /mnt/recovery and compare the stored /mnt/recovery files checksums to its rootfs counterpart. 
If the files differ, the respective rootfs file will be overwritten by the respective recovery file
In the event of an update, the respective recovery files will be overwritten by the respective rootfs file
