* ~~Remove callback from peripheral.listen, make it an event handler for 'connect'~~
* Fill in the rest of the peripheral methods
    * ~~Error~~
    * ~~MTU~~
    * ~~Find Info~~
    * ~~Find by Type~~
    * ~~Read by Type~~
    * ~~Read~~
    * Read Blob
    * Read Multi
    * ~~Read by Group~~
    * ~~Write~~
    * ~~Write Cmd~~
    * Prep Write
    * Exec Write
    * ~~Handle Value Confirmation~~
    * Signed Write
* ~~Get rid of type lookups in peripheral, in favor of linear search of handles array~~
* ~~Fix reconnect~~
* ~~Have characteristics emit event when value is written externally~~
* Have characteristics notify/indicate when value is written internally
* Clean up the debug logging
* Clean up the requires
* Figure out why access to hci0 requires root, but nothing else does
* Remove all Bluez code
* Run valgrind on code
