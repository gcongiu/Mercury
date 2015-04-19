Mercury v0.2

Description
-----------
Mercury is a transparent guided I/O framework able to feed hints into the I/O stack transparently to the user. Users can use mercury to guide the caching behaviour of the file system, adapting it the to application's needs. For example, an application reading data from a file non-contiguously, that is, reads may be interleaved by gaps (seek) inevitably leading to inefficient utilization of the storage device (HDD), can benefit from Mercury that can prefetch a bigger block of sequential data from the file system into the cache and allow the application to access non-contiguous data from local main memory (this is based on the concept of data sieving). The user can also set POSIX hints (advice) describing the access pattern of the application for different regions of the file (e.g. disabling read-ahead for randomly accessed regions).

Requires
--------
During the development phase the following libraries and environments were used: libjsoncpp-dev, libpcre++-dev, liblog4cpp5-dev, Linux 2.6 or later, g++-4.4 or later

Installing mercury
------------------

   * Run "./configure", "make", and "make install".

   * Make sure AdviceManager and libAIO.so are installed in the system paths.

   * Alternatively you can install them in a path of your choice and afterwards:

      a) add a new entry in /etc/ld.so.conf.d/mercury.conf that includes the non-standard path to libAIO.so and run ldconfig to update the dynamic linker cache.

      b) add the non-standard path to AdviceManager to the PATH variable by typing: export PATH=$PATH:/path/to/AdviceManager.

Mercury Architecture
---------------

   * Mercury is composed of three components: the "AdviceManager", the "AIO" library and the "Json configuration file".

   * AssistedIO (AIO): is the Assisting I/O library overloading the POSIX I/O methods. The library has to be linked to the application or alternatively interposed using the dynamic linker "LD_PRELOAD" variable. The AIO library will listen to the I/O requests made by the application and will forward all the open/read/close operations to the AdviceManager (which will assist it in the I/O) through UNIX domain sockets using "sendmsg()".

   * AdviceManager: is an independent process that receives I/O requests info from the AIO library and generates hints for the file system in the form of POSIX advice through "posix_fadvise()" or GPFS hints through "gpfs_fcntl()". The hints interface is selected automatically depending on where the file resides. The hints to be passed to the file system are specified by the user in a separate configuration file that is loaded at the time the AdviceManager and the AIO library are started.

   * Config file: The configuration file is a structured file containing hints information for the AdviceManager and the AIO library. This information does not depend from the implementation used by the AdviceManager to submit them to the file system, but is general. Follows an example of configuration file that can be used:

                       {
                           "File": [
                           {
                               "Path": "full_path_name_for_file",
                               "BlockSize": 4096,
                               "CacheSize": 16,
                               "ReadAheadSize": 4,
                               "WillNeed": [
                               {
                                   "Offset": 0,
                                   "Length": 10240
                               }],
                               "Random": [
                               {
                                   "Offset": 10240,
                                   "Length": 10240
                               }],
                               "Sequential": [
                               {
                                   "Offset": 20480,
                                   "Length": 0
                               }]
                           }],
                           "Directory": [
                           {
                               "Path": "full_path_name_for_dir",
                               "Sequential": [
                               {
                                   "Offset": 0,
                                   "Length": 0
                               }]
                           }]
                       }

    Besides access pattern information, the configuration file also contains information to adapt the behavior of AdviceManager to different files. In particular, "BlockSize" controls the size of the data block that will be prefetched in "WillNeed" (if 0 default is 4MB), "CacheSize" controls how many data blocks will be kept in the block cache (AdvisorManager cache) at the same time (if 0 default is 16) and finally, "ReadAheadSize" controls how many blocks will be prefetched by the AdviceManager starting from the current block (if 0 default is 3). Note that while for POSIX hints "BlockSize" can be set arbitrarily, in the case of GPFS this has to be set to the GPFS block size (in any case it will be reset internally to match this).
The accessed block in the AdviceManager cache will be handled using a LRU algorithm, releasing the older blocks to make space for new ones. Please note that for GPFS users cannot make the cache size too big since GPFS needs you to release the blocks that are no longer needed in order to keep accepting new hints.

   * SelfAssistedIO (SAIO): with Mercury version 0.2 a new implementation of the I/O library is available. This version provides applications using the library with self assisted hints support (without the need of the AdviceManager process). This implementation does not support hints synchronization among different processes accessing the same file. The main reason this implementation is provided is because GPFS doesn't allow a process to hint multiple files at the same time (which is the typical case with the AdviceManager).

Running Applications
--------------------
The AdviceManager & I/O libs will search for the configuration file looking in the environmental variable "MERCURY_CONFIG", thus make sure you have exported it by typing:

                          "export MERCURY_CONFIG=path_to_config_file"

After you have started the AdviceManager you can start the target application. If you don't want to recompile your application (e.g. you don't have the source code for it) use the "LD_PRELOAD" variable to interpose the AIO library between your application and LibC by typing:

                                 "export LD_PRELOAD=libAIO.so"   

Stop Mercury
------------
The AdviceManager can be stopped using, e.g., socat by typing the following command:

                   "socat -u EXEC:"echo \"Shutdown\"" UNIX-SENDTO:/tmp/channel"

Lustre POSIX_FADV_WILLNEED Support
----------------------------------

A linux kernel patch is provided to make the LUSTRE distributed file system work with Mercury prefetching functionalities (i.e. WillNeed). The patch is targeted to kernel version 2.6.32-358.23.2.el6.
The provided patch modifies the VFS in the Linux kernel to make 'fadvise64()' call 'aio_read()' operation instead of '__do_page_cache_readahead()'. The 'aio_read()' call is later on intercepted by the Lustre Lite module in the virtual file system and handled as normal read operation, acquiring the locks as appropriate.
NOTE: at the moment Lustre Lite is not modified and this has effects on the amount of data actually read by 'posix_fadvise()'. In fact Lustre read-ahead is not disabled and thus more data than requested is eventually loaded into the page cache.

Follows a simplified call graph for the read operation in the linux kernel. The picture is divided in four quadrants, according to the type of operation (Page cache or File operations) and the specific implementation (Lustre Lite, Linux Kernel). As it can be seen, 'fadvise64()' has no effect on Lustre read page (ll_readpage). Thus, POSIX_FADV_WILLNEED is discarded by the Kernel. On the other hand, by making 'fadvise64()' call 'aio_read()' will automatically end up invoking 'll_file_read()' and thus 'll_readpage()'.

     F                                                   |
     i                                                   |        ll_file_read()
     l                                                   |              |
     e                                                   |              v
                                                         |      ll_file_aio_read()
     o                                                   |              |
     p                                                   |              v
     e                                                   |     ll_file_io_generic()
     r                                                   |              |
     a                                                   |              v
     t               +----> generic_file_aio_read() <------- lustre_generic_file_read()
     i               |                |                  |
     o               |    no O_DIRECT |                  |
     s               |                v                  |
                     |      do_generic_file_read()       |
                     |                |                  |
             --------|----------------|------------------+------------------------------------------
                     |                |                  |
     P               |                v              page fault
     a               +--------- find_get_page() ----------------> ll_readpage()
     g               ^                |   ^              |              |
     e               |     page fault |   |              |              v
                     |                v   +-----------+  |      cl_io_read_page()
     o               |   page_cache_sync_readahead()  |  |              |
     p               |                |               |  |              v
     e               |   FMODE_RANDOM |               |  |       cio_read_page()
     r               |                v               |  |      vvp_io_read_page()
     a  fadvise64() ---> force_page_cache_readahead() |  |              |
     t       ^       |                |               |  |              v
     i       |       |                |               +--------- ll_readahead()
     o       |       |                v                  |
     n       +-------+--__do_page_cache_readahead()      |
     s                                                   |
                                 Linux Kernel            |         Lustre Lite 
