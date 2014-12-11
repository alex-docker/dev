ZeroTier Control Plane
======

This code is responsible for the local command bus used to control the ZeroTier One service on a local machine via zerotier-cli or the Qt GUI. It's not part of the core node implementation. It uses Unix domain sockets on unix-like OSes and named pipes on Windows. Authentication is via a simple token mechanism. (Eventually this part of the software is getting a rework.)