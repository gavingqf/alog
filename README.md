# alog
synchronous/asynchronous log
## example
``` 实例:
  void main() {
	  // init log module
	  anet::log::initLog("./log", "gate");

	  // set log level.
	  if (argc >= 2) {
		anet::log::setLogLevel(anet::log::eLogLevel(atoi(argv[1])));
	  } else {
		anet::log::setLogLevel(anet::log::eLogLevel::debug);
	  }

	  // log out synchronously.
	  debug("{}", "=== start ===");
      
      // log out asynchronously(A prefix).
      ADebug("this is %s example", "alog");

      // the same as ADebug, just support with {}.
      Adebug("this is {} example", "alog");

      // release log.
	  anet::log::releaseLog();
    }
```