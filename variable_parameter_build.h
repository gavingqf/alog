#pragma once

/*
 * variadic template building.
 */

#include <vector>
#include <string>
#include "stream_string.h"

namespace anet {
	namespace log {
		// user can use delim as parameter flag.
		static const char *DELIM = "{}";
		static constexpr int string_max_size = 1024;
		using SStreamType = SStreamSpace::StreamStringUnlimit<string_max_size>;

		// split function.
		static void split(const char *pData, const std::string &delim, std::vector<std::string> &vec) {
			if (!pData) { return; }
			const char *next = strstr(pData, delim.c_str());
			if (next) {
				vec.push_back(std::string(pData, next - pData));
				// whether it is over.
				const char *pNext = next + delim.size();
				if (pNext < (pData + strlen(pData))) {
					split(pNext, delim, vec);
				}
			} else {
				vec.push_back(pData);
			}
		}

		/* ============================================================== */
		// recurse call back.
		static void _sm_log_output(SStreamType &ss, const std::vector<std::string>& format, 
			size_t& index) {
			(void)ss;
			(void)index;
			(void)format;
		}

		template<typename T, typename... Args>
		static void _sm_log_output(SStreamType &ss, const std::vector<std::string>& format, 
			size_t& index, const T& first, Args&&... rest
		) {
			if (index < format.size()) {
				ss << format[index];
				ss << first;
				_sm_log_output(ss, format, ++index, std::forward<Args>(rest)...);
			}
		}

		// global function declare: variable_log.
		template<typename... Args>
		inline void variable_log(SStreamType &ss, const char* format, Args&&... args) {
			std::vector<std::string> vec;
			vec.reserve(8);
			split(format, DELIM, vec);

			// variadic parameter format.
			size_t index = 0;
			_sm_log_output(ss, vec, index, std::forward<Args>(args)...);

			// after dealer.
			for (; index < vec.size(); ++index) {
				ss << vec[index];
			}
		}
		/*===============================================================*/
	}
}