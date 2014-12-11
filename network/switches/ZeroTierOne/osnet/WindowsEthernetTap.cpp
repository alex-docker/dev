/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>
#include <winreg.h>
#include <wchar.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>
#include <nldef.h>
#include <netioapi.h>
#include <atlbase.h>
#include <netlistmgr.h>
#include <nldef.h>

#include <iostream>

#include "../node/Constants.hpp"

#include "WindowsEthernetTap.hpp"
#include "WindowsEthernetTapFactory.hpp"
#include "../node/Utils.hpp"
#include "../node/Mutex.hpp"

#include "..\windows\TapDriver\tap-windows.h"

// ff:ff:ff:ff:ff:ff with no ADI
static const ZeroTier::MulticastGroup _blindWildcardMulticastGroup(ZeroTier::MAC(0xff),0);

#define ZT_WINDOWS_CREATE_FAKE_DEFAULT_ROUTE

namespace ZeroTier {

// Only create or delete devices one at a time
static Mutex _systemTapInitLock;

WindowsEthernetTap::WindowsEthernetTap(
	const char *pathToHelpers,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *desiredDevice,
	const char *friendlyName,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg) :
	EthernetTap("WindowsEthernetTap",mac,mtu,metric),
	_handler(handler),
	_arg(arg),
	_nwid(nwid),
	_tap(INVALID_HANDLE_VALUE),
	_injectSemaphore(INVALID_HANDLE_VALUE),
	_pathToHelpers(pathToHelpers),
	_run(true),
	_initialized(false),
	_enabled(true)
{
	char subkeyName[4096];
	char subkeyClass[4096];
	char data[4096];
	char tag[24];

	if (mtu > 2800)
		throw std::runtime_error("MTU too large for Windows tap");

	Mutex::Lock _l(_systemTapInitLock);

	HKEY nwAdapters;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
		throw std::runtime_error("unable to open registry key for network adapter enumeration");

	std::set<std::string> existingDeviceInstances;
	std::string mySubkeyName;

	// We "tag" registry entries with the network ID to identify persistent devices
	Utils::snprintf(tag,sizeof(tag),"%.16llx",(unsigned long long)nwid);

	// Look for the tap instance that corresponds with this network
	for(DWORD subkeyIndex=0;;++subkeyIndex) {
		DWORD type;
		DWORD dataLen;
		DWORD subkeyNameLen = sizeof(subkeyName);
		DWORD subkeyClassLen = sizeof(subkeyClass);
		FILETIME lastWriteTime;
		if (RegEnumKeyExA(nwAdapters,subkeyIndex,subkeyName,&subkeyNameLen,(DWORD *)0,subkeyClass,&subkeyClassLen,&lastWriteTime) == ERROR_SUCCESS) {
			type = 0;
			dataLen = sizeof(data);
			if (RegGetValueA(nwAdapters,subkeyName,"ComponentId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
				data[dataLen] = '\0';
				if (!strnicmp(data,"zttap",5)) {
					std::string instanceId;
					type = 0;
					dataLen = sizeof(data);
					if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
						instanceId.assign(data,dataLen);
						existingDeviceInstances.insert(instanceId);
					}

					std::string instanceIdPath;
					type = 0;
					dataLen = sizeof(data);
					if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
						instanceIdPath.assign(data,dataLen);

					if ((_netCfgInstanceId.length() == 0)&&(instanceId.length() != 0)&&(instanceIdPath.length() != 0)) {
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
							data[dataLen] = '\0';
							if (!strcmp(data,tag)) {
								_netCfgInstanceId = instanceId;
								_deviceInstanceId = instanceIdPath;

								mySubkeyName = subkeyName;
								break; // found it!
							}
						}
					}
				}
			}
		} else break; // no more subkeys or error occurred enumerating them
	}

	// If there is no device, try to create one
	bool creatingNewDevice = (_netCfgInstanceId.length() == 0);
	if (creatingNewDevice) {
		// Log devcon output to a file
		HANDLE devconLog = CreateFileA((_pathToHelpers + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if (devconLog != INVALID_HANDLE_VALUE)
			SetFilePointer(devconLog,0,0,FILE_END);

		// Execute devcon to install an instance of the Microsoft Loopback Adapter
		STARTUPINFOA startupInfo;
		startupInfo.cb = sizeof(startupInfo);
		if (devconLog != INVALID_HANDLE_VALUE) {
			SetFilePointer(devconLog,0,0,FILE_END);
			startupInfo.hStdOutput = devconLog;
			startupInfo.hStdError = devconLog;
		}
		PROCESS_INFORMATION processInfo;
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _pathToHelpers + WindowsEthernetTapFactory::WINENV.devcon + "\" install \"" + _pathToHelpers + WindowsEthernetTapFactory::WINENV.tapDriver + "\" zttap200").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			RegCloseKey(nwAdapters);
			if (devconLog != INVALID_HANDLE_VALUE)
				CloseHandle(devconLog);
			throw std::runtime_error(std::string("unable to find or execute devcon at ") + WindowsEthernetTapFactory::WINENV.devcon);
		}
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);

		if (devconLog != INVALID_HANDLE_VALUE)
			CloseHandle(devconLog);

		// Scan for the new instance by simply looking for taps that weren't originally there...
		for(DWORD subkeyIndex=0;;++subkeyIndex) {
			DWORD type;
			DWORD dataLen;
			DWORD subkeyNameLen = sizeof(subkeyName);
			DWORD subkeyClassLen = sizeof(subkeyClass);
			FILETIME lastWriteTime;
			if (RegEnumKeyExA(nwAdapters,subkeyIndex,subkeyName,&subkeyNameLen,(DWORD *)0,subkeyClass,&subkeyClassLen,&lastWriteTime) == ERROR_SUCCESS) {
				type = 0;
				dataLen = sizeof(data);
				if (RegGetValueA(nwAdapters,subkeyName,"ComponentId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
					data[dataLen] = '\0';
					if (!strnicmp(data,"zttap",5)) {
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
							if (existingDeviceInstances.count(std::string(data,dataLen)) == 0) {
								RegSetKeyValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",REG_SZ,tag,(DWORD)(strlen(tag)+1));
								_netCfgInstanceId.assign(data,dataLen);
								type = 0;
								dataLen = sizeof(data);
								if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
									_deviceInstanceId.assign(data,dataLen);
								mySubkeyName = subkeyName;

								// Disable DHCP by default on newly created devices
								HKEY tcpIpInterfaces;
								if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
									DWORD enable = 0;
									RegSetKeyValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),"EnableDHCP",REG_DWORD,&enable,sizeof(enable));
									RegCloseKey(tcpIpInterfaces);
								}

								break; // found it!
							}
						}
					}
				}
			} else break; // no more keys or error occurred
		}
	}

	if (_netCfgInstanceId.length() > 0) {
		char tmps[64];
		unsigned int tmpsl = Utils::snprintf(tmps,sizeof(tmps),"%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",(unsigned int)mac[0],(unsigned int)mac[1],(unsigned int)mac[2],(unsigned int)mac[3],(unsigned int)mac[4],(unsigned int)mac[5]) + 1;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"NetworkAddress",REG_SZ,tmps,tmpsl);
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"MAC",REG_SZ,tmps,tmpsl);
		DWORD tmp = mtu;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"MTU",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));

		tmp = 0;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"*NdisDeviceType",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		tmp = IF_TYPE_ETHERNET_CSMACD;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"*IfType",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));

		if (creatingNewDevice) {
			tmp = 0;
			RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"EnableDHCP",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		}
		RegCloseKey(nwAdapters);	
	} else {
		RegCloseKey(nwAdapters);	
		throw std::runtime_error("unable to find or create tap adapter");
	}

	// Convert device GUID junk... blech... is there an easier way to do this?
	{
		char nobraces[128];
		const char *nbtmp1 = _netCfgInstanceId.c_str();
		char *nbtmp2 = nobraces;
		while (*nbtmp1) {
			if ((*nbtmp1 != '{')&&(*nbtmp1 != '}'))
				*nbtmp2++ = *nbtmp1;
			++nbtmp1;
		}
		*nbtmp2 = (char)0;
		if (UuidFromStringA((RPC_CSTR)nobraces,&_deviceGuid) != RPC_S_OK)
			throw std::runtime_error("unable to convert instance ID GUID to native GUID (invalid NetCfgInstanceId in registry?)");
	}

	// Look up interface LUID... why are there (at least) four fucking ways to refer to a network device in Windows?
	if (ConvertInterfaceGuidToLuid(&_deviceGuid,&_deviceLuid) != NO_ERROR)
		throw std::runtime_error("unable to convert device interface GUID to LUID");

	if (friendlyName)
		setFriendlyName(friendlyName);

	// Start background thread that actually performs I/O
	_injectSemaphore = CreateSemaphore(NULL,0,1,NULL);
	_thread = Thread::start(this);

	// Certain functions can now work (e.g. ips())
	_initialized = true;
}

WindowsEthernetTap::~WindowsEthernetTap()
{
	_run = false;
	ReleaseSemaphore(_injectSemaphore,1,NULL);
	Thread::join(_thread);
	CloseHandle(_injectSemaphore);
	_disableTapDevice();
}

void WindowsEthernetTap::setEnabled(bool en)
{
	_enabled = en;
}

bool WindowsEthernetTap::enabled() const
{
	return _enabled;
}

bool WindowsEthernetTap::addIP(const InetAddress &ip)
{
	if (!_initialized)
		return false;
	if (!ip.netmaskBits()) // sanity check... netmask of 0.0.0.0 is WUT?
		return false;

	std::set<InetAddress> haveIps(ips());

	try {
		// Add IP to interface at the netlink level if not already assigned.
		if (!haveIps.count(ip)) {
			MIB_UNICASTIPADDRESS_ROW ipr;

			InitializeUnicastIpAddressEntry(&ipr);
			if (ip.isV4()) {
				ipr.Address.Ipv4.sin_family = AF_INET;
				ipr.Address.Ipv4.sin_addr.S_un.S_addr = *((const uint32_t *)ip.rawIpData());
				ipr.OnLinkPrefixLength = ip.port();
				if (ipr.OnLinkPrefixLength >= 32)
					return false;
			} else if (ip.isV6()) {
				ipr.Address.Ipv6.sin6_family = AF_INET6;
				memcpy(ipr.Address.Ipv6.sin6_addr.u.Byte,ip.rawIpData(),16);
				ipr.OnLinkPrefixLength = ip.port();
				if (ipr.OnLinkPrefixLength >= 128)
					return false;
			} else return false;

			ipr.PrefixOrigin = IpPrefixOriginManual;
			ipr.SuffixOrigin = IpSuffixOriginManual;
			ipr.ValidLifetime = 0xffffffff;
			ipr.PreferredLifetime = 0xffffffff;

			ipr.InterfaceLuid = _deviceLuid;
			ipr.InterfaceIndex = _getDeviceIndex();

			if (CreateUnicastIpAddressEntry(&ipr) == NO_ERROR) {
				haveIps.insert(ip);
			} else {
				return false;
			}
		}

		std::vector<std::string> regIps(_getRegistryIPv4Value("IPAddress"));
		if (std::find(regIps.begin(),regIps.end(),ip.toIpString()) == regIps.end()) {
			std::vector<std::string> regSubnetMasks(_getRegistryIPv4Value("SubnetMask"));
			regIps.push_back(ip.toIpString());
			regSubnetMasks.push_back(ip.netmask().toIpString());
			_setRegistryIPv4Value("IPAddress",regIps);
			_setRegistryIPv4Value("SubnetMask",regSubnetMasks);
		}
		//_syncIpsWithRegistry(haveIps,_netCfgInstanceId);
	} catch ( ... ) {
		return false;
	}
	return true;
}

bool WindowsEthernetTap::removeIP(const InetAddress &ip)
{
	if (!_initialized)
		return false;
	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			for(DWORD i=0;i<ipt->NumEntries;++i) {
				if (ipt->Table[i].InterfaceLuid.Value == _deviceLuid.Value) {
					InetAddress addr;
					switch(ipt->Table[i].Address.si_family) {
						case AF_INET:
							addr.set(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
							break;
						case AF_INET6:
							addr.set(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
							if (addr.isLinkLocal())
								continue; // can't remove link-local IPv6 addresses
							break;
					}
					if (addr == ip) {
						DeleteUnicastIpAddressEntry(&(ipt->Table[i]));
						FreeMibTable(ipt);

						std::vector<std::string> regIps(_getRegistryIPv4Value("IPAddress"));
						std::vector<std::string> regSubnetMasks(_getRegistryIPv4Value("SubnetMask"));
						std::string ipstr(ip.toIpString());
						for(std::vector<std::string>::iterator rip(regIps.begin()),rm(regSubnetMasks.begin());((rip!=regIps.end())&&(rm!=regSubnetMasks.end()));++rip,++rm) {
							if (*rip == ipstr) {
								regIps.erase(rip);
								regSubnetMasks.erase(rm);
								_setRegistryIPv4Value("IPAddress",regIps);
								_setRegistryIPv4Value("SubnetMask",regSubnetMasks);
								break;
							}
						}

						return true;
					}
				}
			}
			FreeMibTable((PVOID)ipt);
		}
	} catch ( ... ) {}
	return false;
}

std::set<InetAddress> WindowsEthernetTap::ips() const
{
	static const InetAddress linkLocalLoopback("fe80::1",64); // what is this and why does Windows assign it?
	std::set<InetAddress> addrs;

	if (!_initialized)
		return addrs;

	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			for(DWORD i=0;i<ipt->NumEntries;++i) {
				if (ipt->Table[i].InterfaceLuid.Value == _deviceLuid.Value) {
					switch(ipt->Table[i].Address.si_family) {
						case AF_INET: {
							InetAddress ip(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
							if (ip != InetAddress::LO4)
								addrs.insert(ip);
						}	break;
						case AF_INET6: {
							InetAddress ip(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
							if ((ip != linkLocalLoopback)&&(ip != InetAddress::LO6))
								addrs.insert(ip);
						}	break;
					}
				}
			}
			FreeMibTable(ipt);
		}
	} catch ( ... ) {} // sanity check, shouldn't happen unless out of memory

	return addrs;
}

void WindowsEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	if ((!_initialized)||(!_enabled)||(_tap == INVALID_HANDLE_VALUE)||(len > (ZT_IF_MTU)))
		return;

	Mutex::Lock _l(_injectPending_m);
	_injectPending.push( std::pair<Array<char,ZT_IF_MTU + 32>,unsigned int>(Array<char,ZT_IF_MTU + 32>(),len + 14) );
	char *d = _injectPending.back().first.data;
	to.copyTo(d,6);
	from.copyTo(d + 6,6);
	d[12] = (char)((etherType >> 8) & 0xff);
	d[13] = (char)(etherType & 0xff);
	memcpy(d + 14,data,len);

	ReleaseSemaphore(_injectSemaphore,1,NULL);
}

std::string WindowsEthernetTap::deviceName() const
{
	char tmp[1024];
	if (ConvertInterfaceLuidToNameA(&_deviceLuid,tmp,sizeof(tmp)) != NO_ERROR)
		return std::string("[ConvertInterfaceLuidToName() failed]");
	return std::string(tmp);
}

void WindowsEthernetTap::setFriendlyName(const char *dn)
{
	if (!_initialized)
		return;
	HKEY ifp;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,(std::string("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\") + _netCfgInstanceId).c_str(),0,KEY_READ|KEY_WRITE,&ifp) == ERROR_SUCCESS) {
		RegSetKeyValueA(ifp,"Connection","Name",REG_SZ,(LPCVOID)dn,(DWORD)(strlen(dn)+1));
		RegCloseKey(ifp);
	}
}

bool WindowsEthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	if (!_initialized)
		return false;
	HANDLE t = _tap;
	if (t == INVALID_HANDLE_VALUE)
		return false;

	std::set<MulticastGroup> newGroups;

	// Ensure that groups are added for each IP... this handles the MAC:ADI
	// groups that are created from IPv4 addresses. Some of these may end
	// up being duplicates of what the IOCTL returns but that's okay since
	// the set<> will filter that.
	std::set<InetAddress> ipaddrs(ips());
	for(std::set<InetAddress>::const_iterator i(ipaddrs.begin());i!=ipaddrs.end();++i)
		newGroups.insert(MulticastGroup::deriveMulticastGroupForAddressResolution(*i));

	// The ZT1 tap driver supports an IOCTL to get multicast memberships at the L2
	// level... something Windows does not seem to expose ordinarily. This lets
	// pretty much anything work... IPv4, IPv6, IPX, oldskool Netbios, who knows...
	unsigned char mcastbuf[TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE];
	DWORD bytesReturned = 0;
	if (DeviceIoControl(t,TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS,(LPVOID)0,0,(LPVOID)mcastbuf,sizeof(mcastbuf),&bytesReturned,NULL)) {
		MAC mac;
		DWORD i = 0;
		while ((i + 6) <= bytesReturned) {
			mac.setTo(mcastbuf + i,6);
			i += 6;
			if ((mac.isMulticast())&&(!mac.isBroadcast())) {
				// exclude the nulls that may be returned or any other junk Windows puts in there
				newGroups.insert(MulticastGroup(mac,0));
			}
		}
	}

	bool changed = false;

	for(std::set<MulticastGroup>::iterator mg(newGroups.begin());mg!=newGroups.end();++mg) {
		if (!groups.count(*mg)) {
			groups.insert(*mg);
			changed = true;
		}
	}
	for(std::set<MulticastGroup>::iterator mg(groups.begin());mg!=groups.end();) {
		if ((!newGroups.count(*mg))&&(*mg != _blindWildcardMulticastGroup)) {
			groups.erase(mg++);
			changed = true;
		} else ++mg;
	}

	return changed;
}

bool WindowsEthernetTap::injectPacketFromHost(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	return false;
}

void WindowsEthernetTap::threadMain()
	throw()
{
	char tapPath[256];
	OVERLAPPED tapOvlRead,tapOvlWrite;
	HANDLE wait4[3];
	char *tapReadBuf = (char *)0;

	// Shouldn't be needed, but Windows does not overcommit. This Windows
	// tap code is defensive to schizoid paranoia degrees.
	while (!tapReadBuf) {
		tapReadBuf = (char *)::malloc(ZT_IF_MTU + 32);
		if (!tapReadBuf)
			Sleep(1000);
	}

	// Tap is in this weird Windows global pseudo file space
	Utils::snprintf(tapPath,sizeof(tapPath),"\\\\.\\Global\\%s.tap",_netCfgInstanceId.c_str());

	/* More insanity: repetatively try to enable/disable tap device. The first
	 * time we succeed, close it and do it again. This is to fix a driver init
	 * bug that seems to be extremely non-deterministic and to only occur after
	 * headless MSI upgrade. It cannot be reproduced in any other circumstance.
	 *
	 * Eventually when ZeroTier has actual money we will have someone create an
	 * NDIS6 tap driver. Yes, we'll likely be cool and open source it. */
	bool throwOneAway = true;
	while (_run) {
		_disableTapDevice();
		Sleep(250);
		if (!_enableTapDevice()) {
			::free(tapReadBuf);
			_enabled = false;
			return; // only happens if devcon is missing or totally fails
		}
		Sleep(250);

		_tap = CreateFileA(tapPath,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED,NULL);
		if (_tap == INVALID_HANDLE_VALUE) {
			Sleep(500);
			continue;
		}

		{
			uint32_t tmpi = 1;
			DWORD bytesReturned = 0;
			DeviceIoControl(_tap,TAP_WIN_IOCTL_SET_MEDIA_STATUS,&tmpi,sizeof(tmpi),&tmpi,sizeof(tmpi),&bytesReturned,NULL);
			bytesReturned = 0;
			DeviceIoControl(_tap,TAP_WIN_IOCTL_SET_MEDIA_STATUS,&tmpi,sizeof(tmpi),&tmpi,sizeof(tmpi),&bytesReturned,NULL);
		}

		{
#ifdef ZT_WINDOWS_CREATE_FAKE_DEFAULT_ROUTE
			/* This inserts a fake default route and a fake ARP entry, forcing
			 * Windows to detect this as a "real" network and apply proper
			 * firewall rules.
			 *
			 * This hack is completely stupid, but Windows made me do it
			 * by being broken and insane.
			 *
			 * Background: Windows tries to detect its network location by
			 * matching it to the ARP address of the default route. Networks
			 * without default routes are "unidentified networks" and cannot
			 * have their firewall classification changed by the user (easily).
			 *
			 * Yes, you read that right.
			 *
			 * The common workaround is to set *NdisDeviceType to 1, which
			 * totally disables all Windows firewall functionality. This is
			 * the answer you'll find on most forums for things like OpenVPN.
			 *
			 * Yes, you read that right.
			 *
			 * The default route workaround is also known, but for this to
			 * work there must be a known default IP that resolves to a known
			 * ARP address. This works for an OpenVPN tunnel, but not here
			 * because this isn't a tunnel. It's a mesh. There is no "other
			 * end," or any other known always on IP.
			 *
			 * So let's make a fake one and shove it in there along with its
			 * fake static ARP entry. Also makes it instant-on and static.
			 *
			 * We'll have to see what DHCP does with this. In the future we
			 * probably will not want to do this on DHCP-enabled networks, so
			 * when we enable DHCP we will go in and yank this wacko hacko from
			 * the routing table before doing so.
			 *
			 * Like Jesse Pinkman would say: "YEEEEAAH BITCH!" */
			const uint32_t fakeIp = htonl(0x19fffffe); // 25.255.255.254 -- unrouted IPv4 block
			for(int i=0;i<8;++i) {
				MIB_IPNET_ROW2 ipnr;
				memset(&ipnr,0,sizeof(ipnr));
				ipnr.Address.si_family = AF_INET;
				ipnr.Address.Ipv4.sin_addr.s_addr = fakeIp;
				ipnr.InterfaceLuid.Value = _deviceLuid.Value;
				ipnr.PhysicalAddress[0] = _mac[0] ^ 0x10; // just make something up that's consistent and not part of this net
				ipnr.PhysicalAddress[1] = 0x00;
				ipnr.PhysicalAddress[2] = (UCHAR)((_deviceGuid.Data1 >> 24) & 0xff);
				ipnr.PhysicalAddress[3] = (UCHAR)((_deviceGuid.Data1 >> 16) & 0xff);
				ipnr.PhysicalAddress[4] = (UCHAR)((_deviceGuid.Data1 >> 8) & 0xff);
				ipnr.PhysicalAddress[5] = (UCHAR)(_deviceGuid.Data1 & 0xff);
				ipnr.PhysicalAddressLength = 6;
				ipnr.State = NlnsPermanent;
				ipnr.IsRouter = 1;
				ipnr.IsUnreachable = 0;
				ipnr.ReachabilityTime.LastReachable = 0x0fffffff;
				ipnr.ReachabilityTime.LastUnreachable = 1;
				DWORD result = CreateIpNetEntry2(&ipnr);
				if (result != NO_ERROR)
					Sleep(500);
				else break;
			}
			for(int i=0;i<8;++i) {
				MIB_IPFORWARD_ROW2 nr;
				memset(&nr,0,sizeof(nr));
				InitializeIpForwardEntry(&nr);
				nr.InterfaceLuid.Value = _deviceLuid.Value;
				nr.DestinationPrefix.Prefix.si_family = AF_INET; // rest is left as 0.0.0.0/0
				nr.NextHop.si_family = AF_INET;
				nr.NextHop.Ipv4.sin_addr.s_addr = fakeIp;
				nr.Metric = 9999; // do not use as real default route
				nr.Protocol = MIB_IPPROTO_NETMGMT;
				DWORD result = CreateIpForwardEntry2(&nr);
				if (result != NO_ERROR)
					Sleep(500);
				else break;
			}
#endif
		}

		if (throwOneAway) {
			throwOneAway = false;
			CloseHandle(_tap);
			_tap = INVALID_HANDLE_VALUE;
			Sleep(1000);
			continue;
		} else break;
	}

	memset(&tapOvlRead,0,sizeof(tapOvlRead));
	tapOvlRead.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	memset(&tapOvlWrite,0,sizeof(tapOvlWrite));
	tapOvlWrite.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	wait4[0] = _injectSemaphore;
	wait4[1] = tapOvlRead.hEvent;
	wait4[2] = tapOvlWrite.hEvent; // only included if writeInProgress is true

	// Start overlapped read, which is always active
	ReadFile(_tap,tapReadBuf,sizeof(tapReadBuf),NULL,&tapOvlRead);
	bool writeInProgress = false;

	for(;;) {
		if (!_run) break;
		DWORD r = WaitForMultipleObjectsEx(writeInProgress ? 3 : 2,wait4,FALSE,5000,TRUE);
		if (!_run) break;

		if ((r == WAIT_TIMEOUT)||(r == WAIT_FAILED))
			continue;

		if (HasOverlappedIoCompleted(&tapOvlRead)) {
			DWORD bytesRead = 0;
			if (GetOverlappedResult(_tap,&tapOvlRead,&bytesRead,FALSE)) {
				if ((bytesRead > 14)&&(_enabled)) {
					MAC to(tapReadBuf,6);
					MAC from(tapReadBuf + 6,6);
					unsigned int etherType = ((((unsigned int)tapReadBuf[12]) & 0xff) << 8) | (((unsigned int)tapReadBuf[13]) & 0xff);
					try {
						Buffer<4096> tmp(tapReadBuf + 14,bytesRead - 14);
						_handler(_arg,from,to,etherType,tmp);
					} catch ( ... ) {} // handlers should not throw
				}
			}
			ReadFile(_tap,tapReadBuf,ZT_IF_MTU + 32,NULL,&tapOvlRead);
		}

		if (writeInProgress) {
			if (HasOverlappedIoCompleted(&tapOvlWrite)) {
				writeInProgress = false;
				_injectPending_m.lock();
				_injectPending.pop();
			} else continue; // still writing, so skip code below and wait
		} else _injectPending_m.lock();

		if (!_injectPending.empty()) {
			WriteFile(_tap,_injectPending.front().first.data,_injectPending.front().second,NULL,&tapOvlWrite);
			writeInProgress = true;
		}

		_injectPending_m.unlock();
	}

	CancelIo(_tap);

	CloseHandle(tapOvlRead.hEvent);
	CloseHandle(tapOvlWrite.hEvent);
	CloseHandle(_tap);
	_tap = INVALID_HANDLE_VALUE;

	::free(tapReadBuf);
}

bool WindowsEthernetTap::_disableTapDevice()
{
	HANDLE devconLog = CreateFileA((_pathToHelpers + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (devconLog != INVALID_HANDLE_VALUE)
		SetFilePointer(devconLog,0,0,FILE_END);

	STARTUPINFOA startupInfo;
	startupInfo.cb = sizeof(startupInfo);
	if (devconLog != INVALID_HANDLE_VALUE) {
		startupInfo.hStdOutput = devconLog;
		startupInfo.hStdError = devconLog;
	}
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo,0,sizeof(STARTUPINFOA));
	memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
	if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _pathToHelpers + WindowsEthernetTapFactory::WINENV.devcon + "\" disable @" + _deviceInstanceId).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		if (devconLog != INVALID_HANDLE_VALUE)
			CloseHandle(devconLog);
		return false;
	}
	WaitForSingleObject(processInfo.hProcess,INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	if (devconLog != INVALID_HANDLE_VALUE)
		CloseHandle(devconLog);

	return true;
}

bool WindowsEthernetTap::_enableTapDevice()
{
	HANDLE devconLog = CreateFileA((_pathToHelpers + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (devconLog != INVALID_HANDLE_VALUE)
		SetFilePointer(devconLog,0,0,FILE_END);

	STARTUPINFOA startupInfo;
	startupInfo.cb = sizeof(startupInfo);
	if (devconLog != INVALID_HANDLE_VALUE) {
		startupInfo.hStdOutput = devconLog;
		startupInfo.hStdError = devconLog;
	}
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo,0,sizeof(STARTUPINFOA));
	memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
	if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _pathToHelpers + WindowsEthernetTapFactory::WINENV.devcon + "\" enable @" + _deviceInstanceId).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		if (devconLog != INVALID_HANDLE_VALUE)
			CloseHandle(devconLog);
		return false;
	}
	WaitForSingleObject(processInfo.hProcess,INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	if (devconLog != INVALID_HANDLE_VALUE)
		CloseHandle(devconLog);

	return true;
}

NET_IFINDEX WindowsEthernetTap::_getDeviceIndex()
{
	MIB_IF_TABLE2 *ift = (MIB_IF_TABLE2 *)0;

	if (GetIfTable2Ex(MibIfTableRaw,&ift) != NO_ERROR)
		throw std::runtime_error("GetIfTable2Ex() failed");

	for(ULONG i=0;i<ift->NumEntries;++i) {
		if (ift->Table[i].InterfaceLuid.Value == _deviceLuid.Value) {
			NET_IFINDEX idx = ift->Table[i].InterfaceIndex;
			FreeMibTable(ift);
			return idx;
		}
	}

	FreeMibTable(&ift);

	throw std::runtime_error("interface not found");
}

std::vector<std::string> WindowsEthernetTap::_getRegistryIPv4Value(const char *regKey)
{
	std::vector<std::string> value;
	HKEY tcpIpInterfaces;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
		char buf[16384];
		DWORD len = sizeof(buf);
		DWORD kt = REG_MULTI_SZ;
		if (RegGetValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),regKey,0,&kt,&buf,&len) == ERROR_SUCCESS) {
			switch(kt) {
				case REG_SZ:
					if (len > 0)
						value.push_back(std::string(buf));
					break;
				case REG_MULTI_SZ: {
					for(DWORD k=0,s=0;k<len;++k) {
						if (!buf[k]) {
							if (s < k) {
								value.push_back(std::string(buf + s));
								s = k + 1;
							} else break;
						}
					}
				}	break;
			}
		}
		RegCloseKey(tcpIpInterfaces);
	}
	return value;
}

void WindowsEthernetTap::_setRegistryIPv4Value(const char *regKey,const std::vector<std::string> &value)
{
	std::string regMulti;
	for(std::vector<std::string>::const_iterator s(value.begin());s!=value.end();++s) {
		regMulti.append(*s);
		regMulti.push_back((char)0);
	}
	HKEY tcpIpInterfaces;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
		if (regMulti.length() > 0) {
			regMulti.push_back((char)0);
			RegSetKeyValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),regKey,REG_MULTI_SZ,regMulti.data(),(DWORD)regMulti.length());
		} else {
			RegDeleteKeyValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),regKey);
		}
		RegCloseKey(tcpIpInterfaces);
	}
}

} // namespace ZeroTier
