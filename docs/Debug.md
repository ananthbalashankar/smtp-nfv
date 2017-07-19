Debugging OpenNetVM
==

Compile with Debugging Symbols
--

When programming NFs or the manager, it is often neccessary to debug. 
By default openNetVM does not compile with debugging symbols but is able to when compiler flags are set. 

For example, you may want to debug [speed tester][speed_tester] with gdb.
- The first step is to set the correct environment variables which can be done using the following command:
  + `export USER_FLAGS="-g -O0"`
    - `-g` produces debugging symbols and `-O0` reduces compiler optimizations to the lowest level
- Now navigate to your debug target and run `make`
  + `cd examples/speed_tester`
  + `make`
- The executable now has debugging symbols. Run it in gdb with the necessary args. For example:
  + `sudo gdb --args ./build/speed_tester -l 5 -n 3 --proc-type=secondary -- -r 1  -- -d 1`
- It is now possible to set breakpoints and perform other gdb operations!

If for some reason `USER_FLAGS` does not set correctly, it's possible to edit the `Makefile` of your debug target and set the flags manually.
It can be done by adding a line similar to this:
- `CFLAGS += -g`

Packet capturing
--
When working with different packet protocols and tcp related applications it is often needed to look at the packets received/sent by the manager. DPDK provides a dpdk-pdump application that can capture packets to a pcap file.  

To use dpdk-pdump set CONFIG_RTE_LIBRTE_PMD_PCAP=y in dpdk/config/common_base and then recompile dpdk.  

Then execute dpdk-pdump as a secondary application when the manager is running
```
cd dpdk/x86_64-native-linuxapp-gcc
sudo ./build/app/pdump/dpdk-pdump -- --pdump 'port=0,queue=*,rx-dev=/tmp/rx.pcap'
```
Full set of options and configurations for dpdk-pdump can be found [here][dpdk-pdump].
 
[dpdk-pdump]: http://dpdk.org/doc/guides/tools/pdump.html#example
[speed_tester]: ../examples/speed_tester