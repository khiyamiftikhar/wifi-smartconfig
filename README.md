This is an espidf component for wifi station
The credentials for wifi are provided through ESPTOUCH APP, which are then stored in NVS.
The total entries that NVS can have is set statically through Kconfig
At boot, first the live APs are scanned and checked whether credentials are available in NVS, and connection attempt is made
If it doesnt succeed, then credentials are read through ESPTOUCH APP and stored in NVS for future use.
