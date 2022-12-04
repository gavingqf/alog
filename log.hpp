#pragma once

/*
 * anet log module for synchronous/asynchronous(prefix A) mode.
 */

#include <memory>
#include <thread>
#include <functional>
#include <string>
#include <vector>
#include <chrono>
#include <assert.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include "variable_parameter_build.h"
#include "semaphore.hpp"


 // for createDir func.
#if defined(_WIN32)
#include "time.h"
#include <sys/timeb.h>
#include <direct.h>
#include <io.h>
#define ACCESS _access
#define MKDIR(a) _mkdir((a))
#else
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#define ACCESS access
#define MKDIR(a) mkdir((a), 0755)
// Linux like os's GetTickCount().
inline auto GetTickCount() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

  // get time info: {second, millisecond}
std::pair<time_t, int> getTimeInfo() {
#if defined(WIN32) || defined(WIN64)
	struct timeb tp;
	ftime(&tp);
	return { tp.time,tp.millitm };
#else
	struct timeval now;
	struct timezone zone;
	gettimeofday(&now, &zone);
	return { now.tv_sec,now.tv_usec / 1000 };
#endif
}

namespace anet {
	namespace log {
		// logger interface
		class ILog {
		public:
			virtual ~ILog() {}
			// log level
			virtual bool setLevel(int level) = 0;
			virtual void Debug(const char *format, ...) = 0;
			virtual void Info(const char *format, ...) = 0;
			virtual void Warn(const char *format, ...) = 0;
			virtual void Crit(const char *format, ...) = 0;
		};

		// all kinds of const variables.
		static constexpr int gPath_max_size = 260;
		static constexpr int gLog_data_size = 1024;
		static constexpr int gLog_max_size = 1024 + 512;
		// write file frequency unit: ms asynchronously.
		static constexpr int gAsyncLogWriteFrequency = 1000;

		// separate the long file to short one.
		const char* shortFileName(const std::string &file) {
			// compatible for windows and Linux fold seperator.
			const char *seperator = "/";
#if defined(_WIN32)
			seperator = "\\";
#else
			seperator = "/";
#endif
			auto pos = file.rfind(seperator);
			const char *pFile = file.c_str();
			if (pos != size_t(-1)) {
				pFile = &file[pos + 1];
			}
			return pFile;
		}

		// get date string
		char* getDateInfo(char(&data)[gLog_data_size]) {
			auto s = getTimeInfo().first;
			struct tm &t = *localtime(&s);
			int n = std::snprintf(data, sizeof(data) - 1,
				"%04d%02d%02d",
				1900 + t.tm_year,
				1 + t.tm_mon,
				t.tm_mday
			); data[n] = '\0';
			return data;
		}

		// get current time format.
		template <size_t N>
		const char* buildCurrentTime(char(&timeInfo)[N]) {
			auto timePair = getTimeInfo();
			auto ms = timePair.second;
			auto s = timePair.first;
			auto tm = localtime(&s);
			int n = std::snprintf(timeInfo, sizeof(timeInfo) - 1,
				"%d-%02d-%02d %02d:%02d:%02d.%03d",
				1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec, ms
			); timeInfo[n] = 0;
			return timeInfo;
		}

		// create directory.
		int createDir(const char *dirPath) {
			int pathLen = int(strlen(dirPath));
			char realTmpDir[gPath_max_size] = { 0 };
			int minPathSize = pathLen > gPath_max_size ? gPath_max_size : pathLen;
			strncpy(realTmpDir, dirPath, minPathSize);

			// append / at tail.
			if (dirPath[pathLen - 1] != '\\' && dirPath[pathLen - 1] != '/') {
				realTmpDir[minPathSize] = '/';
				realTmpDir[minPathSize + 1] = '\0';
			}

			// create if not exists it.
			int createRet = ACCESS(realTmpDir, 0);
			if (createRet != 0) {
				createRet = MKDIR(realTmpDir);
				if (createRet != 0) {
					return -1;
				}
			}

			return 0;
		}

		// log level enum.
		enum class eLogLevel : int {
			debug = 0,
			info,
			warn,
			crit,
			allLevelSize,
		};

		// file format 
		const char *gLog_out_format = "%s [%s] %s";

		// my log implementation, which can be used outside.
		class aLog : public ILog {
		public:
			explicit aLog(const std::string &foldName, const std::string &prefix) :
				m_logFold(foldName), m_prefix(prefix) {
				bool ret = initLog();
				m_queue.reserve(1024);
				assert(ret && "create log error");
			}
			explicit aLog(const std::string &foldName) : aLog(foldName, "") {
			}
			~aLog() {
				release_log();
			}
			aLog(const aLog &rhs) = delete;
			const aLog& operator=(const aLog &rhs) = delete;

		public:
			// build variable parameters.
        #define BuildVariableFunc(fmt,level,args,ss) \
           if (!checkLevel(level)) {                 \
			     return;                             \
           }                                         \
		   variable_log(ss, fmt, std::forward<Args>(args)...);

			// support {} as parameter.
			// synchronous and asynchronous mode.
			template <typename... Args>
			void debug(const char *fmt, Args&&... args) {
				SStreamType ss;                        
				BuildVariableFunc(fmt, eLogLevel::debug, args, ss);
				this->Debug(ss.str());
			}
			template <typename... Args>
			void Adebug(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::debug, args, ss);
				this->ADebug(ss.str());
			}
			// warn
			template <typename... Args>
			void warn(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::warn, args, ss);
				this->Warn(ss.str());
			}
			template <typename... Args>
			void Awarn(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::warn, args, ss);
				this->AWarn(ss.str());
			}

			// info
			template <typename... Args>
			void info(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::info, args, ss);
				this->Info(ss.str());
			}
			template <typename... Args>
			void Ainfo(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::info, args, ss);
				this->AInfo(ss.str());
			}

			// crit
			template <typename... Args>
			void crit(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::crit, args, ss);
				this->Crit(ss.str());
			}
			template <typename... Args>
			void Acrit(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::crit, args, ss);
				this->ACrit(ss.str());
			}

		public:
			virtual bool setLevel(int level) override {
				if (level > int(eLogLevel::crit) || 
					level < int(eLogLevel::debug)) {
					return false;
				}
				m_logLevel = eLogLevel(level);
				return true;
			}
			eLogLevel getLevel() const {
				return m_logLevel;
			}

			// build variadic parameter macro, adding \n at tail.
        #define buildFuncParameter(fmt,myPrintfBuf,myBufferSize) {\
			va_list args;                                \
			va_start(args,fmt);                          \
			int n = std::vsnprintf(myPrintfBuf,sizeof(myPrintfBuf)-1,fmt,args);\
			if (n <= int(sizeof(myPrintfBuf) - 2)) {     \
				(myPrintfBuf)[n] = '\n';                 \
				(myPrintfBuf)[n+1] = 0;                  \
			} else {                                     \
				(myPrintfBuf)[(myBufferSize) - 2] = '\n';\
				(myPrintfBuf)[(myBufferSize) - 1] = 0;   \
			}                                            \
			va_end(args);                                \
		  } // end of macro.

			  // output message with level and time information synchronously.
        #define LevelOutput(fmt,level) {        \
            if (!checkLevel(level)) {           \
			    return;                         \
            }                                   \
			                                    \
		    char timeInfo[128];                 \
		    buildCurrentTime(timeInfo);         \
				                                \
		    char myPrintfBuf[gLog_data_size];   \
		    buildFuncParameter(fmt, myPrintfBuf, gLog_data_size);\
				                                \
		    char allBuff[gLog_max_size];        \
		    std::snprintf(allBuff, sizeof(allBuff) - 1, gLog_out_format, timeInfo, getLevelInfo(level), myPrintfBuf); \
		    this->write(allBuff);               \
          }

			// output message with level and time information asynchronously.
        #define ALevelOutput(fmt,level) {       \
            if (!checkLevel(level)) {           \
			    return;                         \
            }                                   \
			                                    \
		    char timeInfo[128];                 \
		    buildCurrentTime(timeInfo);         \
				                                \
		    char myPrintfBuf[gLog_data_size];   \
		    buildFuncParameter(fmt, myPrintfBuf, gLog_data_size);\
				                                \
		    char allBuff[gLog_max_size];        \
		    std::snprintf(allBuff, sizeof(allBuff) - 1, gLog_out_format, timeInfo, getLevelInfo(level), myPrintfBuf); \
		    this->pushQueue(allBuff);           \
          }

		public:
			// synchronous interfaces.
			virtual void Debug(const char *fmt, ...) override {
				LevelOutput(fmt, eLogLevel::debug);
			}
			virtual void Info(const char *fmt, ...) override {
				LevelOutput(fmt, eLogLevel::info);
			}
			virtual void Warn(const char *fmt, ...) override {
				LevelOutput(fmt, eLogLevel::warn);
			}
			virtual void Crit(const char *fmt, ...) override {
				LevelOutput(fmt, eLogLevel::crit);
			}

			// asynchronous interfaces
			void ADebug(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::debug);
			}
			void AInfo(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::info);
			}
			void AWarn(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::warn);
			}
			void ACrit(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::crit);
			}

		protected:
			void pushQueue(const std::string &msg) {
				m_asyncMutex.lock();
				m_queue.append(std::move(msg));
				m_asyncMutex.unlock();
				// signal that the semaphore is ready.
				m_sem.signal();
			}

			// thread function to write the queue's message to file.
			void threadFunc() {
				std::string swapQueue;
				swapQueue.reserve(1024);
				long long lastLogTime = GetTickCount();
				while(!m_quit) {
					// wait for notify or time out after the delayTime time.
					m_sem.wait_for(std::chrono::milliseconds(gAsyncLogWriteFrequency));
	
					// real time check.
					auto nowTime = GetTickCount();
			        if (nowTime - lastLogTime < gAsyncLogWriteFrequency) {
						continue;
					}
					lastLogTime = nowTime;

					// do write log.
					this->tryToWrite(swapQueue);
				}

				// try to write all log messages if the thread exits.
				this->tryToWrite(swapQueue);
			}
			inline void tryToWrite(std::string& swapQueue) {
				{// swap queue lock.
					std::lock_guard<std::mutex> lg(m_asyncMutex);
					if (m_queue.empty()) {
						return ;
					}
					m_queue.swap(swapQueue);
				}

				// write to log file.
				this->doWriteLog(swapQueue);
				swapQueue.clear();
			}

			inline void doWriteLog(const std::string &allMsg) {
				this->write(allMsg.c_str());
			}

			// whether is the same (year,month,day,hour) date.
			bool isTheSameDate() const {
				auto timePair = getTimeInfo();
				auto s = timePair.first;
				auto tm = localtime(&s);
				return m_year == tm->tm_year &&
					m_month == tm->tm_mon &&
					m_day == tm->tm_mday &&
					m_hour == tm->tm_hour;
			}

			// write content to log file synchronously.
			void write(const char *content) {
				if (content == nullptr) {
					return;
				}

				std::lock_guard<std::mutex> guard(m_mutex);
				// check whether the date is changed.
				if (!isTheSameDate()) {
					// close before file.
					if (m_fileStream != nullptr) {
						fclose(m_fileStream);
						m_fileStream = nullptr;
					}
					// create subFold;
					char data[gLog_data_size];
					auto &&subFold = m_logFold + "/" + getDateInfo(data);
					createDir(subFold.c_str());
					// create log file.
					createFile();
				}

				// window's output
#ifdef _WIN32
				printf("%s", content);
#endif
				// log file output
				fputs(content, m_fileStream);
				fflush(m_fileStream);
			}

			inline const char* getLevelInfo(eLogLevel level) const {
				return m_levels[int(level)];
			}

			bool initLog() {
				// create directory
				if (createDir(m_logFold.c_str()) < 0) {
					return false;
				}

				// create sub directory.
				char dateBuff[gLog_data_size];
				auto &&subFold = m_logFold + "/" + getDateInfo(dateBuff);
				if (createDir(subFold.c_str()) < 0) {
					return false;
				}

				// create log file.
				if (createFile()) {
					// start thread to deal asynchronous log
					this->m_th = std::make_unique<std::thread>(std::bind(&aLog::threadFunc, this));
					return true;
				} else {
					return false;
				}
			}

			// create file.
			bool createFile() {
				char fileName[gPath_max_size];

				// save time.
				auto s = getTimeInfo().first;
				auto tm = localtime(&s);

				// record time info.
				m_year = tm->tm_year;
				m_month = tm->tm_mon;
				m_day = tm->tm_mday;
				m_hour = tm->tm_hour;

				/* file name */
				char data[gLog_data_size];
				int n = std::snprintf(fileName,
					sizeof(fileName) - 1,
					"%s/%s/%s%04d%02d%02d_%02d.log",
					m_logFold.c_str(),
					getDateInfo(data),
					m_prefix.c_str(),
					1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
					tm->tm_hour
				); fileName[n] = 0;
				m_fileStream = fopen(fileName, "a+");
				return m_fileStream != nullptr;
			}

			// release me
			void release_log() {
				// let's the logging thread exit first.
				m_quit = true;
				m_th->join();

				m_mutex.lock();
				if (m_fileStream != nullptr) {
					fclose(m_fileStream);
					m_fileStream = nullptr;
				}
				m_mutex.unlock();
			}

			bool checkLevel(eLogLevel level) const {
				return m_logLevel <= level;
			}

		private:
			// file handle and its mutex.
			FILE* m_fileStream;
			mutable std::mutex m_mutex;

			// log level info.
			const char* m_levels[int(eLogLevel::allLevelSize)] = { "debg","info","warn","crit" };
			eLogLevel m_logLevel{ eLogLevel::debug };

			// log time info(year,month,day,hour).
			int m_year;
			int m_month;
			int m_day;
			int m_hour;
			std::string    m_logFold;
			std::string    m_prefix;

			// asynchronous log info
			std::unique_ptr<std::thread> m_th;
			anet::utils::CSemaphore m_sem;
			mutable std::mutex m_asyncMutex;
			std::string m_queue;
			bool m_quit{ false };
		}; // end of aLog class


		// global log pointer, please initLog() to init it if you wish to use it.
		static aLog *myLog = nullptr;

		// init log module.
		void initLog(std::string foldName, std::string logName,
			eLogLevel level = eLogLevel::debug
		) {
			myLog = new aLog(foldName, logName);
			myLog->setLevel(int(level));
		}

		// set log level
		void setLogLevel(eLogLevel level) {
			if (myLog == nullptr) {
				return;
			}
			myLog->setLevel(int(level));
		}

		// release log module.
		void releaseLog() {
			if (myLog != nullptr) {
				delete myLog;
				myLog = nullptr;
			}
		}
	} // end of log namespace.

	// user can not use the global log module.
	// log format: Debug, Warn, Info, Crit.
	// year-month-day hour:minute:second [level] file function:line message

	// traditional form
#define Debug(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::debug) \
        anet::log::myLog->Debug("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define Warn(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::warn) \
	    anet::log::myLog->Warn("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define Info(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::info) \
        anet::log::myLog->Info("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define Crit(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::crit) \
        anet::log::myLog->Crit("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }

	  // ==asynchronous mode ==
#define ADebug(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::crit) \
        anet::log::myLog->ADebug("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define AWarn(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::crit) \
        anet::log::myLog->AWarn("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define AInfo(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::crit) \
        anet::log::myLog->AInfo("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define ACrit(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::crit) \
        anet::log::myLog->ACrit("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }

	  // === {} format ===
#define debug(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::debug) \
        anet::log::myLog->debug("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define warn(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::warn) \
        anet::log::myLog->warn("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define info(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::info) \
        anet::log::myLog->info("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define crit(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::crit) \
        anet::log::myLog->crit("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}

#define Adebug(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::debug) \
        anet::log::myLog->Adebug("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define Awarn(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::warn) \
        anet::log::myLog->Awarn("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define Ainfo(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::info) \
        anet::log::myLog->Ainfo("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define Acrit(fmt,...) { \
      if (anet::log::myLog != nullptr && anet::log::myLog->getLevel() <= anet::log::eLogLevel::crit) \
        anet::log::myLog->Acrit("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
} // end of anet namespace