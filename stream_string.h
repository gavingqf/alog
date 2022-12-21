#pragma once

/*
 * stream string class
 */

#include <string>
#include "string.h"
#include <assert.h>
#include <vector>

namespace SStreamSpace {
	constexpr size_t default_buffer_init_size = 1024;
	constexpr size_t default_buffer_grow_size = 64;

	// align
	inline int Align(int nSize, int nAlignSize) {
		return (nSize + nAlignSize - 1) & (~(nAlignSize - 1));
	}
	inline int Align(int nSize) {
		return Align(nSize, sizeof(void*));
	}

#ifndef myMax
  #define myMax(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef myMin
  #define myMin(a,b)            (((a) < (b)) ? (a) : (b))
#endif

	// grow size
#define grow(nNeedSize, nDefaultGrowSize)    {             \
	int nSize1 = Align(nNeedSize);                         \
	int nSize2 = Align(nDefaultGrowSize);                  \
	m_vecBuffer.resize(m_pos + myMax(nSize1, nSize2), 0);  \
}

	// size check
#define sizeCheck(nNeedSize) {                                 \
	if (int(m_pos + (nNeedSize)) >= int(m_vecBuffer.size())) { \
		grow((nNeedSize)+1, default_buffer_grow_size);         \
	}                                                          \
}

	// unlimited stream string object
	template <size_t SIZE>
	class StreamStringUnlimit {
	public:
		StreamStringUnlimit() : m_pos(0) {		
			m_vecBuffer.resize(Align(SIZE), 0);
		}
		virtual ~StreamStringUnlimit() {
			m_pos = 0;
		}

	public:
		bool empty() const {
			return m_vecBuffer.empty();
		}

		size_t length() const {
			return m_vecBuffer.size();
		}

		char operator [](size_t index) {
			if (index >= m_vecBuffer.size()) {
				return 0;
			}
			return m_vecBuffer[index];
		}

		// const char*
		const char* str() const {
			if (m_vecBuffer.empty()) {
				return "";
			} else {
			    return &m_vecBuffer[0];
			}
		}

		std::string string() {
			return  std::string(str());
		}

		// char* data
		const char* data() {
			if (m_vecBuffer.empty()) {
				return "";
			} else {
			    return &m_vecBuffer[0];
			}
		}

		// len
		int len() const {
			return m_pos;
		}

		void Reset() {
			m_vecBuffer.clear();
			m_vecBuffer.resize(Align(SIZE), 0);
			m_pos = 0;
		}

		operator const char*() {
			return this->str();
		}

		operator char*() {
			return this->data();
		}
        
	public:
		template<typename T>
		StreamStringUnlimit &operator += (T data) {
			*this << data;
			return *this;
		}

		// for all integer values as std::to_string();
		template<typename T>
		StreamStringUnlimit &operator << (T data) {
			auto &&value = std::to_string(data);
			this->To(value.c_str(), value.length());
			return *this;
		}

		// void*, just for pointer address.
		template <typename T>
		StreamStringUnlimit &operator << (T* pData) {
			const T *pSrc = pData;
			const char *pString = nullptr;
			const char* format = "%p";
			int size = sizeof(void*);
			int flag = 0;
			if (!pSrc) { // special for null if NULL pointer.
				format = "%s";
				pString = "null";
				size = int(strlen(pString));
				flag = 1;
			}
			sizeCheck(size);
			sprintf(&m_vecBuffer[m_pos], format, (flag == 0? pData : pSrc));
			m_pos += size;

			return *this;
		}
        
		// all kinds of string values.
		template<int N>
		StreamStringUnlimit &operator << (char (&szData)[N]) {
			return this->To(szData, strlen(szData));
		}
		StreamStringUnlimit &operator << (const std::string &strData) {
			return this->To(strData.c_str(), strData.size());
		}
		StreamStringUnlimit &operator << (const char* szData) {
			if (!szData) szData = "null";
			return this->To(szData, strlen(szData));
		}
		StreamStringUnlimit &operator << (char* szData) {
			return this->To(szData, strlen(szData));
		}
		template <int N>
		StreamStringUnlimit &operator << (const StreamStringUnlimit<N>& oData) {
			return this->To(oData.str(), oData.len());
		}

		// add ' and ' for string types.
		template<int N>
		StreamStringUnlimit& operator += (char (&data)[N]) {
			return (*this) << "'" << data << "'";
		}
		StreamStringUnlimit& operator += (const char *data) {
			return (*this) << "'" << data << "'";
		}
		StreamStringUnlimit& operator += (char *data) {
			return (*this) << "'" << data << "'";
		}
		StreamStringUnlimit& operator += (const std::string &data) {
			return (*this) << "'" << data << "'";
		}

		StreamStringUnlimit &AddZero() {
			sizeCheck(2);
			m_vecBuffer[m_pos++] = 0;
			return *this;
		}

		virtual StreamStringUnlimit &To(const char *data, size_t dateLen) {
			// size check.
			sizeCheck(int(dateLen));
			// just copy it
			memcpy(&m_vecBuffer[m_pos], data, dateLen);
			m_pos += int(dateLen);

			return *this;
		}

	protected:
		// write index.
		int m_pos;

		// buffer.
		std::vector<char> m_vecBuffer;
	};

	// stream string with out buffer.
	class StreamStringex {
	public:
		explicit StreamStringex(char *pszBuff, int nBuffSize, const char *pszInitBuff = nullptr) {
			m_pBuf = pszBuff;
			m_maxSize = nBuffSize;
			assert(m_pBuf && "buffer is null");

			memset(m_pBuf, 0, nBuffSize);

			// init buff.
			if (pszInitBuff) {
				this->To(pszInitBuff, strlen(pszInitBuff));
			} else {
				m_pos = 0;
			}
		}

		virtual ~StreamStringex() {
			m_pBuf = nullptr;
			m_maxSize = 0;
			m_pos = 0;
		}

	public:
		void set_len(int pos) {
			m_pos = pos;
		}

		// const char*
		const char* str() const {
			return m_pBuf;
		}

		// char* data
		char* data() {
			return m_pBuf;
		}

		// len
		int len() const {
			return m_pos;
		}

		void Reset() {
			if (m_pBuf) {
				memset(m_pBuf, 0, m_maxSize);
			}
			m_pos = 0;	
		}

		operator const char*() {
			return m_pBuf;
		}

		operator char*() {
			return m_pBuf;
		}

		template<typename T>
		StreamStringex &operator + (T tData) {
			return ((*this) << tData);
		}

		// as integer data
		template<typename T>
		StreamStringex &operator << (T nData) {
			auto strValue = std::to_string(nData);
			this->To(strValue.c_str(), strValue.length());
			return *this;
		}

		template<int N>
		StreamStringex &operator << (char (&szData)[N]) {
			return this->To(szData, strlen(szData));
		}
		StreamStringex &operator << (const std::string &strData) {
			return this->To(strData.c_str(), strData.size());
		}
		StreamStringex &operator << (const char* szData) {
			return this->To(szData, strlen(szData));
		}
		StreamStringex &operator << (char* szData) {
			return this->To(szData, strlen(szData));
		}
		StreamStringex &operator << (const StreamStringex &oData) {
			return this->To(oData.str(), oData.len());
		}

		template<int N>
		StreamStringex& operator += (char (&szData)[N]) {
			(*this) << "'" << szData << "'";
			return *this;
		}
		StreamStringex& operator += (const std::string &strData) {
			(*this) << "'" << strData << "'";
			return *this;
		}
		StreamStringex& operator += (const char* szData) {
			(*this) << "'" << szData << "'";
			return *this;
		}
		StreamStringex& operator += (char* szData) {
			(*this) << "'" << szData << "'";
			return *this;
		}

		StreamStringex &AddZero() {
			if (m_pos >= m_maxSize) {
				return *this;
			}
			m_pBuf[m_pos++] = 0;
			return *this;
		}

	public:
		int GetLeftBuffSize() const {
			return m_maxSize - m_pos;
		}

	protected:
		StreamStringex &To(const char *szData, size_t nDataLen) {
			if (m_pos >= m_maxSize) {
				return *this;
			}

			// just copy it
			memcpy(m_pBuf + m_pos, szData, nDataLen);
			m_pos += int(nDataLen);
			return *this;
		}

    protected:
		// outer buffer
		char *m_pBuf;

		// outer buffer size
		int m_maxSize;

		// buffer position
		int m_pos;
	};

	// fix buffer stream
	template<int SIZE>
	class StreamString {
	public:
		StreamString() {
			Reset();
		}
		virtual ~StreamString() {
			m_Pos = 0;
		}

		// const char*
		const char* str() const {
			return m_szBuf;
		}

		// char* data
		char* data() {
			return m_szBuf;
		}

		// len
		int len() const {
			return m_Pos;
		}

		operator const char*() {
			return m_szBuf;
		}

		operator char*() {
			return m_szBuf;
		}

		char operator[](int o) {
			if (o < SIZE) {
				return m_szBuf[o];
			} else {
				return 0;
			}
		}

		void Reset() {
			memset(m_szBuf, 0, sizeof(m_szBuf));
			m_Pos = 0;
		}

		bool full() const {
			return m_Pos >= SIZE;
		}

		template<typename T>
		StreamStringex &operator + (T tData) {
			return ((*this) << tData);
		}

		// as integer data
		template <typename T>
		StreamString &operator << (T nData) {
			auto &&strValue = std::string(nData);
			this->To(strValue.c_str(), strValue.length());
			return *this;
		}

		template <int N>
		StreamString &operator << (const StreamString<N> &oSS) {
			if (m_Pos + oSS.len() >= SIZE) {
				assert(false && "size less");
				return *this;
			}
			return this->To(oSS.str(), oSS.len());
		}

		template<int N>
		StreamString &operator << (const char (&szData)[N]) {
			return  this->To(szData, strlen(szData));
		}
		StreamString &operator << (const std::string &strData) {
			return this->To(strData.c_str(), strData.size());
		}
		StreamString &operator << (const char* szData) {
			return this->To(szData, strlen(szData));
		}
		StreamString &operator << (char* szData) {
			return this->To(szData, strlen(szData));
		}

		// add ' and ' for string types.
		template<int N>
		StreamString& operator += (const char (&szData)[N]) {
			return  (*this) << "'" << szData << "'";
		}
		StreamString& operator += (const std::string &strData) {
			return  (*this) << "'" << strData << "'";
		}
		StreamString& operator += (const char* szData) {
			return  (*this) << "'" << szData << "'";
		}
		StreamString& operator += (char* szData) {
			return  (*this) << "'" << szData << "'";
		}

		StreamString &AddZero() {
			if (full()) {
				return *this;
			}
			m_szBuf[m_Pos++] = 0;
			return *this;
		}

	protected:
		StreamString &To(const char *szData, size_t nDataLen) {
			if (m_Pos + nDataLen >= SIZE) {
				assert(false && "size less");
				return *this;
			}
			// just copy it
			memcpy(m_szBuf + m_Pos, szData, nDataLen);
			m_Pos += nDataLen;
			return *this;
		}

	private:
		// buffer
		char m_szBuf[SIZE];

		// index
		int  m_Pos;
	};

	// common stream string. just use it outside.
	// variable size stream
	typedef StreamStringUnlimit<1024> unlimit_streamstring;

	// fix size stream string.
	typedef StreamString<1024> limit_streamstring;

#undef grow
#undef sizeCheck
#undef myMax
#undef myMin
}
