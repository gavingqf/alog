#pragma once

/*
* alog is a log module which supports synchronous and asynchronous mode, and it is thread safe.
*
* Copyright (C) 2021-2022 gavingqf(gavingqf@126.com)
*
*    Distributed under the Boost Software License, Version 1.0.
*    (See accompanying file LICENSE_1_0.txt or copy at
*    http://www.boost.org/LICENSE_1_0.txt)
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
#include "time.hpp"

 // for createDir func.
#if defined(_WIN32)
#include <windows.h>
#include "time.h"
#include <sys/timeb.h>
#include <direct.h>
#include <io.h>
#define ACCESS _access
#define MKDIR(a) _mkdir((a))
#pragma warning(disable:4996)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#define ACCESS access
#define MKDIR(a) mkdir((a), 0755)
#endif

  // get time info: {second, millisecond}
inline std::pair<time_t, int> getTimeInfo() {
#if defined(_WIN32) || defined(_WIN64)
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
		// all kinds of const variables.
		static constexpr int gPath_max_size = 260;
		static constexpr int gLog_data_size = 1024;
		static constexpr int gLog_max_size = 1024 + 512;
		// write file frequency unit: ms asynchronously.
		static constexpr int gAsyncLogWriteFrequency = 1000;

		// log's asynchronous queue size.
		static constexpr int gQueueSize = 1024;

		// separate the long file to short one.
		inline const char* shortFileName(const std::string &file) {
			// compatible for windows and Linux fold separator.
			const char *separator = "/";
        #if defined(_WIN32)
			separator = "\\";
        #else
			separator = "/";
        #endif
			auto pos = file.rfind(separator);
			const char *pFile = file.c_str();
			if (pos != size_t(-1)) {
				pFile = &file[pos + 1];
			}
			return pFile;
		}

		// get date string as year-month-day.
		inline char* getDateInfo(char(&data)[gLog_data_size]) {
			auto s = getTimeInfo().first;
			struct tm &t = *localtime(&s);
			int n = std::snprintf(data, sizeof(data),
				"%04d%02d%02d",
				1900 + t.tm_year,
				1 + t.tm_mon,
				t.tm_mday
			);
			(void)n;
			assert(n > 0 && n <= int(sizeof(data)));
			return data;
		}

		// get current time.
		template <size_t N>
		inline const char* buildCurrentTime(char(&timeInfo)[N]) {
			auto timePair = getTimeInfo();
			auto ms = timePair.second;
			auto s = timePair.first;
			auto tm = localtime(&s);
			int n = std::snprintf(timeInfo, sizeof(timeInfo),
				"%d-%02d-%02d %02d:%02d:%02d.%03d",
				1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec, ms
			);
			assert(n > 0 && n <= int(sizeof(timeInfo)));
			return timeInfo;
		}

		// create directory.
		inline int createDir(const char *dirPath) {
			int pathLen = int(strlen(dirPath));
			char realTmpDir[gPath_max_size] = { 0 };
			int minPathSize = pathLen >= gPath_max_size ? gPath_max_size : pathLen;
			strncpy(realTmpDir, dirPath, size_t(minPathSize));

			// append "/" at tail the fold.
			if (realTmpDir[minPathSize - 1] != '\\' && realTmpDir[minPathSize - 1] != '/') {
				if (minPathSize >= gPath_max_size - 1) {
					realTmpDir[gPath_max_size - 2] = '/';
					realTmpDir[gPath_max_size-1] = 0;
				} else {
					realTmpDir[minPathSize] = '/';
					realTmpDir[minPathSize+1] = 0;
				}
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
			debugLevel = 0,
			infoLevel,
			warnLevel,
			critLevel,
			allLevelSize,
		};

		// file format 
		static const char *gLog_out_format = "%s [%s] %s";

		// log implementation, which can be used outside.
		class aLog final {
		public:
			explicit aLog(const std::string& filePath, const std::string& prefix, int asyncWriteTime) {
				this->setLogInfo(filePath, prefix, asyncWriteTime);
			}
			explicit aLog(const std::string &filePath) : 
				aLog(filePath, "log", gAsyncLogWriteFrequency) {
			}
			aLog() {}
			~aLog() {
				release_log();
			}
			aLog(const aLog &rhs) = delete;
			aLog& operator=(const aLog &rhs) = delete;

		public:
			static aLog& instance();

			// init log
			bool setLogInfo(const std::string& filePath, const std::string& prefix, 
				int asyncWriteTime) {
				m_logFilePath = filePath;
				m_prefix = prefix;
				m_quit = false;
				m_asyncToFileMs = asyncWriteTime;
				if (m_asyncToFileMs <= 0) {
					m_asyncToFileMs = gAsyncLogWriteFrequency;
				}
				m_queue.reserve(gQueueSize);
				return initLog();
			}

			// support base type of T.
			// now just support the debug mode. 
			template <typename T>
			aLog& operator << (const T& t) {
				using streamType = SStreamSpace::StreamStringUnlimit<64>;
				streamType oss;
				oss << t;
				this->debug("%s", oss.str());
				return *this;
			}

		public:
			// build variable parameters.
            #define BuildVariableFunc(fmt,level,args,ss) {        \
               if (!checkLevel(level)) {                          \
			       return;                                        \
               }                                                  \
		       variable_log(ss, fmt, std::forward<Args>(args)...);\
		    }

			// support {} as parameter.
			// synchronous and asynchronous mode.
			template <typename... Args>
			void debug(const char *fmt, Args&&... args) {
				SStreamType ss;                        
				BuildVariableFunc(fmt, eLogLevel::debugLevel, args, ss);
				this->Debug(ss.str());
			}
			template <typename... Args>
			void Adebug(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::debugLevel, args, ss);
				this->ADebug(ss.str());
			}
			// warn
			template <typename... Args>
			void warn(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::warnLevel, args, ss);
				this->Warn(ss.str());
			}
			template <typename... Args>
			void Awarn(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::warnLevel, args, ss);
				this->AWarn(ss.str());
			}

			// info
			template <typename... Args>
			void info(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::infoLevel, args, ss);
				this->Info(ss.str());
			}
			template <typename... Args>
			void Ainfo(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::infoLevel, args, ss);
				this->AInfo(ss.str());
			}

			// crit
			template <typename... Args>
			void crit(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::critLevel, args, ss);
				this->Crit(ss.str());
			}
			template <typename... Args>
			void Acrit(const char *fmt, Args&&... args) {
				SStreamType ss;
				BuildVariableFunc(fmt, eLogLevel::critLevel, args, ss);
				this->ACrit(ss.str());
			}

		public:
			bool setLevel(int level) {
				if (level > int(eLogLevel::critLevel) || level < int(eLogLevel::debugLevel)) {
					return false;
				}
				m_logLevel = eLogLevel(level);
				return true;
			}
			int getLevel() const {
				return int(m_logLevel);
			}

			// build variadic parameter macro, adding "\n" at tail.
        #define buildFuncParameter(fmt,myPrintfBuf,myBufferSize) {\
			va_list args;                                \
			va_start(args,fmt);                          \
			int n = std::vsnprintf(myPrintfBuf,(myBufferSize)-1,fmt,args);\
            if (n < 0 || n > myBufferSize) return ;      \
                                                         \
			if (n <= int((myBufferSize) - 2)) {          \
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
		    std::snprintf(allBuff, sizeof(allBuff), gLog_out_format, timeInfo, getLevelInfo(level), myPrintfBuf); \
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
		    std::snprintf(allBuff, sizeof(allBuff)-1, gLog_out_format, timeInfo, getLevelInfo(level), myPrintfBuf); \
		    this->pushQueue(allBuff);           \
          }

		public:
			// synchronous interfaces.
			void Debug(const char *fmt, ...) {
				LevelOutput(fmt, eLogLevel::debugLevel);
			}
			void Info(const char *fmt, ...) {
				LevelOutput(fmt, eLogLevel::infoLevel);
			}
			void Warn(const char *fmt, ...) {
				LevelOutput(fmt, eLogLevel::warnLevel);
			}
			void Crit(const char *fmt, ...) {
				LevelOutput(fmt, eLogLevel::critLevel);
			}

			// asynchronous interfaces
			void ADebug(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::debugLevel);
			}
			void AInfo(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::infoLevel);
			}
			void AWarn(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::warnLevel);
			}
			void ACrit(const char *fmt, ...) {
				ALevelOutput(fmt, eLogLevel::critLevel);
			}

		protected:
			// pushQueue pushes log message to the asynchronous queue.
			void pushQueue(const std::string &msg) {
				m_asyncMutex.lock();
				m_queue.append(msg);
				m_asyncMutex.unlock();

				// signal that the semaphore is ready.
				m_sem.signal();
			}

			// thread function to write the queue's message to the local file.
			void threadFunc() {
				std::string swapQueue;
				swapQueue.reserve(gQueueSize);
				long long lastLogTime = GetNowMSTime();
				while (!m_quit) {
					// wait for notify or time out after the delayTime time.
					m_sem.wait_for(std::chrono::milliseconds(m_asyncToFileMs));
	
					// real time check.
					auto nowTime = GetNowMSTime();
			        if (nowTime - lastLogTime < m_asyncToFileMs) {
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
					auto &&subFold = m_logFilePath + "/" + getDateInfo(data);
					if (createDir(subFold.c_str()) < 0) {
						return;
					}

					// create a log file.
					if (!createFile()) {
						return;
					}
				}

				// window's output
                #ifdef _WIN32
				  printf("%s", content);
                #endif

				assert(m_fileStream != nullptr && "file stream is nullptr");
				if (m_fileStream != nullptr) {
					// log file output
					fputs(content, m_fileStream);
					fflush(m_fileStream);
				}
			}

			inline const char* getLevelInfo(eLogLevel level) const {
				return m_levels[int(level)];
			}

			// initLog initializes the log module.
			bool initLog() {
				// create directory
				if (createDir(m_logFilePath.c_str()) < 0) {
					return false;
				}

				// create sub directory.
				char dateBuff[gLog_data_size];
				auto &&subFold = m_logFilePath + "/" + getDateInfo(dateBuff);
				if (createDir(subFold.c_str()) < 0) {
					return false;
				}

				// create the log file.
				if (createFile()) {
					m_th = std::make_unique<std::thread>(std::bind(&aLog::threadFunc, this));
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
					sizeof(fileName),
					"%s/%s/%s%04d%02d%02d_%02d.log",
					m_logFilePath.c_str(),
					getDateInfo(data),
					m_prefix.c_str(),
					1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
					tm->tm_hour
				);
				assert(n > 0 && n <= int(sizeof(fileName)));
				m_fileStream = fopen(fileName, "a+");
				return m_fileStream != nullptr;
			}

			// release me
			void release_log() {
				// let's the logging thread exit first.
				m_quit = true;
				m_th->join();

				// then close file handler.
				m_mutex.lock();
				if (m_fileStream != nullptr) {
					fclose(m_fileStream);
					m_fileStream = nullptr;
				}
				m_mutex.unlock();
			}

			inline bool checkLevel(eLogLevel level) const {
				return m_logLevel <= level;
			}

		private:
			// file handle and its mutex.
			FILE* m_fileStream{ nullptr };
			mutable std::mutex m_mutex;

			// log level info.
			const char* m_levels[int(eLogLevel::allLevelSize)] = { "debg","info","warn","crit" };
			eLogLevel m_logLevel{ eLogLevel::debugLevel };

			// log time info(year,month,day,hour).
			int m_year;
			int m_month;
			int m_day;
			int m_hour;
			std::string    m_logFilePath;
			std::string    m_prefix;

			// asynchronous log info
			std::unique_ptr<std::thread> m_th;
			anet::utils::CSemaphore m_sem;
			mutable std::mutex m_asyncMutex;
			std::string m_queue;
			bool m_quit{ false };

			// the frequency(unit:ms) to write message to file handler.
			int m_asyncToFileMs{ gAsyncLogWriteFrequency };
		}; // end of aLog class


		// initLog initializes the log module,
		// where foldName is log's output fold, logName is log name,
		// level is default log level, 
		// and asyncWriteMSTime is the time to write to log handler.
		inline bool initLog(std::string foldName, std::string logName,
			eLogLevel level = eLogLevel::debugLevel,
			int asyncWriteMSTime = gAsyncLogWriteFrequency
		) {
			if (!aLog::instance().setLogInfo(foldName, logName, asyncWriteMSTime)) {
				return false;
			}
			return aLog::instance().setLevel(int(level));
		}

		// setLogLevel sets log level
		inline bool setLogLevel(eLogLevel level) {
			return aLog::instance().setLevel(int(level));
		}

		// releaseLog releases log module.
		inline void releaseLog() {}
             ///////////////////////////////////////////////////////
               ///////////////////////////////////////////////////
               // the following macros can be visited outside.  //
               ///////////////////////////////////////////////////
             ///////////////////////////////////////////////////////
#define LoggerDebug(log,fmt,...) { \
      if (log != nullptr && log->getLevel() <= int(anet::log::eLogLevel::debugLevel)) \
        log->Debug("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define LoggerWarn(log,fmt,...) { \
      if (log != nullptr && log->getLevel() <= int(anet::log::eLogLevel::warnLevel)) \
	    log->Warn("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LoggerInfo(fmt,...) { \
      if (log != nullptr && log->.getLevel() <= int(anet::log::eLogLevel::infoLevel)) \
        log->Info("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LoggerCrit(log,fmt,...) { \
      if (log != nullptr && log->getLevel() <= int(anet::log::eLogLevel::critLevel)) \
        log->Crit("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }

      // traditional form
#define LogDebug(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::debugLevel)) \
        anet::log::aLog::instance().Debug("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define LogWarn(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::warnLevel)) \
	    anet::log::aLog::instance().Warn("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LogInfo(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::infoLevel)) \
        anet::log::aLog::instance().Info("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LogCrit(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::critLevel)) \
        anet::log::aLog::instance().Crit("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }

	  // ==asynchronous mode ==
#define LogADebug(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::debugLevel)) \
        anet::log::aLog::instance().ADebug("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LogAWarn(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::warnLevel)) \
        anet::log::aLog::instance().AWarn("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LogAInfo(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::infoLevel)) \
        anet::log::aLog::instance().AInfo("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define LogACrit(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::critLevel)) \
        anet::log::aLog::instance().ACrit("%s %s:%d " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__); }

	  // === {} format ===
	  /*synchronous mode*/
#define Logdebug(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::debugLevel)) \
        anet::log::aLog::instance().debug("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define Logwarn(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::warnLevel)) \
        anet::log::aLog::instance().warn("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define Loginfo(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::infoLevel)) \
        anet::log::aLog::instance().info("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define Logcrit(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::critLevel)) \
        anet::log::aLog::instance().crit("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}

	  /*asynchronous mode*/
#define LogAdebug(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::debugLevel)) \
        anet::log::aLog::instance().Adebug("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define LogAwarn(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::warnLevel)) \
        anet::log::aLog::instance().Awarn("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define LogAinfo(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::infoLevel)) \
        anet::log::aLog::instance().Ainfo("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
#define LogAcrit(fmt,...) { \
      if (anet::log::aLog::instance().getLevel() <= int(anet::log::eLogLevel::critLevel)) \
        anet::log::aLog::instance().Acrit("{} {}:{} " fmt, anet::log::shortFileName(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__);}
	
    } // end of the log namespace.
} // end of anet namespace
		  