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

#ifndef ZT_ETHERNETTAP_HPP
#define ZT_ETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <set>

#include "Constants.hpp"
#include "MAC.hpp"
#include "InetAddress.hpp"
#include "Buffer.hpp"
#include "MulticastGroup.hpp"
#include "NonCopyable.hpp"

namespace ZeroTier {

/**
 * Base class for Ethernet tap device implementations
 */
class EthernetTap : NonCopyable
{
protected:
	EthernetTap(const char *cn,const MAC &m,unsigned int mt,unsigned int met) :
		_implName(cn),
		_mac(m),
		_mtu(mt),
		_metric(met) {}

public:
	virtual ~EthernetTap() {}

	/**
	 * @return Implementation class name (e.g. UnixEthernetTap)
	 */
	inline const char *implementationName() const { return _implName; }

	/**
	 * Sets whether device is 'up'
	 *
	 * This may do nothing on some platforms.
	 *
	 * @param en Is device enabled?
	 */
	virtual void setEnabled(bool en) = 0;

	/**
	 * @return Is device 'up'?
	 */
	virtual bool enabled() const = 0;

	/**
	 * @return MAC address of this interface
	 */
	inline const MAC &mac() const throw() { return _mac; }

	/**
	 * @return MTU of this interface
	 */
	inline unsigned int mtu() const throw() { return _mtu; }

	/**
	 * @return Interface metric
	 */
	inline unsigned int metric() const throw() { return _metric; }

	/**
	 * Add an IP to this interface
	 *
	 * @param ip IP and netmask (netmask stored in port field)
	 * @return True if IP added successfully
	 */
	virtual bool addIP(const InetAddress &ip) = 0;

	/**
	 * Remove an IP from this interface
	 *
	 * Link-local IP addresses may not be able to be removed, depending on platform and type.
	 *
	 * @param ip IP and netmask (netmask stored in port field)
	 * @return True if IP removed successfully
	 */
	virtual bool removeIP(const InetAddress &ip) = 0;

	/**
	 * @return All IP addresses (V4 and V6) assigned to this interface (including link-local)
	 */
	virtual std::set<InetAddress> ips() const = 0;

	/**
	 * Put a frame, making it available to the OS for processing
	 *
	 * @param from MAC address from which frame originated
	 * @param to MAC address of destination (typically MAC of tap itself)
	 * @param etherType Ethernet protocol ID
	 * @param data Frame payload
	 * @param len Length of frame
	 */
	virtual void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len) = 0;

	/**
	 * @return OS-specific device or connection name (e.g. zt0, tap0, etc.)
	 */
	virtual std::string deviceName() const = 0;

	/**
	 * Change this device's user-visible name (if supported)
	 *
	 * @param friendlyName New name
	 */
	virtual void setFriendlyName(const char *friendlyName) = 0;

	/**
	 * Fill or modify a set to contain multicast groups for this device
	 *
	 * This populates a set or, if already populated, modifies it to contain
	 * only multicast groups in which this device is interested.
	 *
	 * This neither includes nor removes the broadcast (ff:ff:ff:ff:ff:ff / 0)
	 * group.
	 *
	 * @param groups Set to modify in place
	 * @return True if set was changed since last call
	 */
	virtual bool updateMulticastGroups(std::set<MulticastGroup> &groups) = 0;

	/**
	 * Inject a packet as if it was sent by the host, if supported
	 *
	 * This is for testing and is typically not supported by real TAP devices.
	 * It's implemented by TestEthernetTap in testnet.
	 *
	 * @param from Source MAC
	 * @param to Destination MAC
	 * @param etherType Ethernet frame type
	 * @param data Packet data
	 * @param len Packet length
	 * @return False if not supported or packet too large
	 */
	virtual bool injectPacketFromHost(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len) = 0;

protected:
	const char *_implName;
	MAC _mac;
	unsigned int _mtu;
	unsigned int _metric;
};

} // namespace ZeroTier

#endif
