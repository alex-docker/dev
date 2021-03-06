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

#ifndef ZT_ATOMICCOUNTER_HPP
#define ZT_ATOMICCOUNTER_HPP

#include "Constants.hpp"
#include "Mutex.hpp"
#include "NonCopyable.hpp"

#ifdef __WINDOWS__
// <atomic> will replace this whole class eventually once it's ubiquitous
#include <atomic>
#endif

namespace ZeroTier {

/**
 * Simple atomic counter supporting increment and decrement
 */
class AtomicCounter : NonCopyable
{
public:
	/**
	 * Initialize counter at zero
	 */
	AtomicCounter()
		throw()
	{
		_v = 0;
	}

	inline operator int() const
		throw()
	{
#ifdef __GNUC__
		return __sync_or_and_fetch(const_cast <volatile int *>(&_v),0);
#else
#ifdef __WINDOWS__
		return (int)_v;
#else
		_l.lock();
		int v = _v;
		_l.unlock();
		return v;
#endif
#endif
	}

	inline int operator++()
		throw()
	{
#ifdef __GNUC__
		return __sync_add_and_fetch(&_v,1);
#else
#ifdef __WINDOWS__
		return ++_v;
#else
		_l.lock();
		int v = ++_v;
		_l.unlock();
		return v;
#endif
#endif
	}

	inline int operator--()
		throw()
	{
#ifdef __GNUC__
		return __sync_sub_and_fetch(&_v,1);
#else
#ifdef __WINDOWS__
		return --_v;
#else
		_l.lock();
		int v = --_v;
		_l.unlock();
		return v;
#endif
#endif
	}

private:
#ifdef __WINDOWS__
	std::atomic_int _v;
#else
	int _v;
#ifndef __GNUC__
	Mutex _l;
#endif
#endif
};

} // namespace ZeroTier

#endif
