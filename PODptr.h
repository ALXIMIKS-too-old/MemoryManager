
#ifndef PODPTR_CLASS
#define	PODPTR_CLASS

#include "spinlock.h"

static const unsigned sleep_invalidate = 25;
static const unsigned bad_invalidate_limit = 45;

#include <atomic>
// my_PODptr - class  containe pointer to allocated data(m_ptr) and allocated total(m_size)
// structure of allocated data (at the begin of allocated data contaned pointer to my_PODptr):
// (!!!__pointer_to_my_PODptr__!!! !!!__allocated_data__!!!) (!!!__pointer_to_my_PODptr__!.....

class my_PODptr
{
	std::atomic<char*> m_ptr;
	volatile size_t m_size;
	std::atomic<char> m_status;

	my_PODptr& operator= (const my_PODptr&) = delete;
	my_PODptr(const my_PODptr&) = delete;
	my_PODptr(my_PODptr&&) = delete;

public:	  
	static const size_t atom_ptr_size;
	my_PODptr() : m_status(0), m_ptr(nullptr), m_size(0){}

	bool valid(const std::memory_order& memory_order = std::memory_order_relaxed) const{
		//! A PODptr object is either valid or invalid.
		//! An invalid PODptr is analogous to a null pointer.
		//! \returns true if PODptr is valid, false if invalid.
		return (data_ptr(memory_order) != nullptr);
	}

	char* try_invalidate(){
		//! Try to Make object invalid.
		//! Return last valid pointer or nullptr when object was changed by other thread
		char* last_ptr = data_ptr(std::memory_order_acquire);
		if (last_ptr != nullptr){ 
			char* tmp_ptr = last_ptr;
			bool compare_result;
			do {
				compare_result = m_ptr.compare_exchange_weak(tmp_ptr, nullptr,
					std::memory_order_release, std::memory_order_relaxed);
			} while (!compare_result && tmp_ptr == last_ptr);
			if (compare_result)
				return last_ptr;
		}
		return nullptr;
	}

	char* invalidate(){
		//! Make object invalid.
		//! Return last valid pointer or nullptr when object was invalided by other thread
		{
			char* last_ptr = data_ptr(std::memory_order_acquire);
			atomic_thread_fence(std::memory_order_acquire);
			if (last_ptr != nullptr){
				char* tmp_ptr = last_ptr;
				bool compare_result;
				do {
					compare_result = m_ptr.compare_exchange_weak(tmp_ptr, nullptr, 
						std::memory_order_release, std::memory_order_acquire);
				} while (!compare_result && tmp_ptr != nullptr);
				if (tmp_ptr != nullptr && compare_result)
					return last_ptr;
			}
		}
		return nullptr;
	}

	char* data_ptr(const std::memory_order& memory_order = std::memory_order_relaxed) const{
		//! Each PODptr keeps the address and m_size of its memory block.
		//! \return The address of its memory block.
		return m_ptr.load(memory_order);
	}

	size_t total_size() const{
		//! Each PODptr keeps the address and m_size of its memory block.		
		//! \returns m_size of the memory block.
		return m_size;
	}

	size_t size()const{
		//! Each PODptr keeps the address and m_size of its memory block.		
		//! \returns m_size of the memory block.
		return m_size - atom_ptr_size;
	}

	const char status(const std::memory_order& memory_order = std::memory_order_relaxed)const{
		// Each PODptr keeps its state 
		return m_status.load(memory_order);
	}

	char* begin()const{
		// pointer of begining of element area
		return data_ptr() - atom_ptr_size;
	}

	void set_total_size(size_t size){
		//! returns total size of element pointer area.
		m_size = size;
	}

	void set_begin(char* ptr_to_begin, const std::memory_order& memory_order = std::memory_order_relaxed){
		//  set_data using pointer of begining of element area
		set_data(ptr_to_begin + atom_ptr_size, memory_order);
	}

	void set_data(char* ptr_to_ptr, const std::memory_order& memory_order = std::memory_order_relaxed){
		// set pointer of element area
		m_ptr.store(ptr_to_ptr, memory_order);
	}

	void set_status(char status, const std::memory_order& memory_order = std::memory_order_relaxed){
		// set state of current PODptr
		m_status.store(status, memory_order);
	}

	static std::atomic<my_PODptr*>* PODptr_using_data(char* data_ptr){
		// 	get atomic pointer to PODptr at the begin of element pointer area using pointer to element area m_ptr 
		return PODptr_using_begin_data(data_ptr - atom_ptr_size);
	}

	static std::atomic<my_PODptr*>* PODptr_using_begin_data(char* begin_ptr){
		// 	set pointer to PODptr at the begin of element pointer area using pointer begining of element area
		return static_cast<std::atomic<my_PODptr*>*>(static_cast<void*>(begin_ptr));
	}

	static void set_PODptr_using_begin_data(my_PODptr* my_ptr, char* begin_ptr,
		const std::memory_order& memory_order = std::memory_order_relaxed){
		// 	set pointer to PODptr at the begin of element pointer area using pointer begining of element area
		std::atomic<my_PODptr*>* pod_ptr = PODptr_using_begin_data(begin_ptr);
		pod_ptr->store(my_ptr, memory_order);
	}

	static void set_PODptr_using_data(my_PODptr* my_ptr, char* data_ptr,
		const std::memory_order& memory_order = std::memory_order_relaxed){
		// 	set pointer to PODptr at the begin of element pointer area using pointer to element area m_ptr
		set_PODptr_using_begin_data(my_ptr, data_ptr - atom_ptr_size, memory_order);
	}
};
const size_t my_PODptr::atom_ptr_size = sizeof(std::atomic<my_PODptr*>);
#endif