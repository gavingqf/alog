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

	  // set start flag
	  debug("{}", "=== start ===");
      ADebug("this is %s example", "alog");
      Adebug("this is {} example", "alog");
    }
```