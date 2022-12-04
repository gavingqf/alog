#pragma once

/*
 * semaphore for synchronization scenario.
 */

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace anet {
	namespace utils {
		// semaphore class.
		class CSemaphore final {
		public:
			explicit CSemaphore(long count = 0) : m_count(count) {}
			virtual ~CSemaphore() = default;
			CSemaphore(const CSemaphore& rhs) = delete;
			CSemaphore& operator=(const CSemaphore& rhs) = delete;

		public:
			void signal() {
				std::unique_lock<std::mutex> guard(m_mu);
				if (++m_count >= 0) {
					m_cv.notify_one();
				}
			}
			void wait() {
				std::unique_lock<std::mutex> guard(m_mu);
				--m_count;
				m_cv.wait(guard, [this]() {
					return m_count >= 0;
				});
			}
			template <typename Predicate>
			void wait(Predicate pred) {
				std::unique_lock<std::mutex> guard(m_mu);
				--m_count;
				m_cv.wait(guard, [this, pred]() {
					return pred() && m_count >= 0;
				});
			}

			template<class _Rep, class _Period>
			bool wait_for(const std::chrono::duration<_Rep, _Period> &s) {
				std::unique_lock<std::mutex> guard(m_mu);
				--m_count;
				return m_cv.wait_for(guard, s, [this]() {
					return m_count >= 0;
				});
			}
			template <class _Rep, class _Period, class Predicate>
			bool wait_for(const std::chrono::duration<_Rep, _Period> &s, Predicate pred) {
				std::unique_lock<std::mutex> guard(m_mu);
				--m_count;
				return m_cv.wait_for(guard, s, [this, pred]() {
					return pred() && m_count >= 0;
				});
			}
			
			long getValue() const {
				std::unique_lock<std::mutex> lock(m_mu);
				return m_count;
			}

		private:
			long m_count;
			mutable std::mutex m_mu;
			std::condition_variable m_cv;
		};
	}
}