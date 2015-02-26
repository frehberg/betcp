# betcp

Best-Effort TCP is making use of Out Of Band (OOB) signaling feature
of TCP. 

In case of network congestions and send-failures, the sender can set a
marker in the outbound stream to signal the receiver which data to be
discarded and where to continue reading.

This might be a usefule feature for VoIP, to realize best effort, time
critical communication with connection oriented TCP instead of UDP.

The core library has been implemented using C only.

To compile the C++ Unit-Tests, the following libaries must be present
on your build host (currently using Ubuntu 14.04) are:

* Boost-threading (currently 1.54)
* STL 
* CppUnit (currently 1.13)

Compilation is done, invoking the following command on command line

$ make

