
#ifndef SPINLOCK_CLASS
#define SPINLOCK_CLASS

#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

// sleep realization
void sleep(unsigned milliseconds){
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}


static const unsigned sleep_acquire = 15;

// mutex based on spinLock could be used with std::lock_guard
class spinLock
{					  
	std::atomic_flag m_locked;
public:
	spinLock(){
		m_locked.clear(); 
	}

	void lock(){
		while (true){
			if (!m_locked.test_and_set(std::memory_order_acquire))
				return;
			sleep(sleep_acquire);
		}
	}
			
	void unlock(){
		m_locked.clear(std::memory_order_release);
	}		
};
#endif