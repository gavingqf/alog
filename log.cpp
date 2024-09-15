#include "log.h"

/*
 * global aLog instance.
 */

namespace anet {
	namespace log {
		aLog& aLog::instance() {
			static aLog gInstance;
			return gInstance;	
		}
	}
}

