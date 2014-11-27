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

#pragma once

#include <stdio.h>

#include "ServiceBase.h"

#include <string>

#include "../../node/Node.hpp"
#include "../../node/Defaults.hpp"
#include "../../node/Thread.hpp"
#include "../../node/Mutex.hpp"
#include "../../node/Utils.hpp"

// Uncomment to make debugging Windows services suck slightly less hard.
//#define ZT_DEBUG_SERVICE "C:\\ZeroTierOneServiceDebugLog.txt"

#ifdef ZT_DEBUG_SERVICE
extern FILE *SVCDBGfile;
extern ZeroTier::Mutex SVCDBGfile_m;
#define ZT_SVCDBG(f,...) { SVCDBGfile_m.lock(); fprintf(SVCDBGfile,f,##__VA_ARGS__); fflush(SVCDBGfile); SVCDBGfile_m.unlock(); }
#else
#define ZT_SVCDBG(f,...) {}
#endif

#define ZT_SERVICE_NAME "ZeroTierOneService"
#define ZT_SERVICE_DISPLAY_NAME "ZeroTier One"
#define ZT_SERVICE_START_TYPE SERVICE_AUTO_START
#define ZT_SERVICE_DEPENDENCIES ""
//#define ZT_SERVICE_ACCOUNT "NT AUTHORITY\\LocalService"
#define ZT_SERVICE_ACCOUNT NULL
#define ZT_SERVICE_PASSWORD NULL

class ZeroTierOneService : public CServiceBase
{
public:
    ZeroTierOneService();
    virtual ~ZeroTierOneService(void);

	/**
	 * Thread main method; do not call elsewhere
	 */
	void threadMain()
		throw();

	// Returns false on failure.
	static bool doStartUpgrade(const std::string &msiPath);

protected:
    virtual void OnStart(DWORD dwArgc, PSTR *pszArgv);
    virtual void OnStop();
	virtual void OnShutdown();

private:
	ZeroTier::Node *volatile _node;
	ZeroTier::Mutex _lock;
	ZeroTier::Thread _thread;
};
