
#ifndef MEMORY_MANAGER
#define MEMORY_MANAGER

#include "PODcontroller.h"


// 	align for allocated mamory (atomic types could give exception from non-align mamory area)
const size_t align_value = sizeof(void*);
inline size_t align(size_t size){
	return ((size - 1) | (align_value - 1)) + 1;
}


// Class for allocate/deallocate memory from pool
// based on two  PODcontroller objects (first for work with allocating, second - with deallocating of memory)
class MemoryManager{
	std::unique_ptr<char[]> m_memory;
	PODcontroller m_alloc;
	PODcontroller m_dealloc;

	MemoryManager& operator= (const MemoryManager&) = delete;
	MemoryManager(const MemoryManager&) = delete;
	MemoryManager(MemoryManager&&) = delete;

public:
	MemoryManager(size_t size) : m_memory(new char[size]), m_alloc(m_memory.get(), size, PODcontroller::ALLOC), m_dealloc(){}

	void* allocate(size_t find_size);
	void deallocate(void* data_void_ptr);
};  

// allocate needed size of memory
void* MemoryManager::allocate(size_t find_size){
	// align of size for atomic types
	size_t need_size = find_size + my_PODptr::atom_ptr_size;
	need_size = align(need_size);

	// search of PODptr object with right element area and invalidate this PODptr object
	std::pair<char*, my_PODptr*> find_result = m_alloc.find_size_in_listed_vector(need_size);

	// check result of search
	char* result = find_result.first;
	if (result == nullptr)
		return nullptr;

	// block of pointer to PODptr at begining of element area
	my_PODptr* pod_ptr = find_result.second;
	pod_ptr->set_PODptr_using_data(nullptr, result);

	// check size of current PODptr	2-nd time (must left more atomic pointer size)
	size_t total_size = pod_ptr->total_size();
	if (pod_ptr->total_size() - need_size > my_PODptr::atom_ptr_size) {

		//	change values of allocated PODptr
		pod_ptr->set_total_size(total_size - need_size);
		pod_ptr->set_PODptr_using_data(pod_ptr, result + need_size);
		pod_ptr->set_data(result + need_size);

		// 	get and change values of deallocated PODptr
		my_PODptr* pod_deall_ptr = m_dealloc.pop();
		pod_deall_ptr->set_total_size(need_size);
		pod_deall_ptr->set_status(PODcontroller::BUSY);
		pod_deall_ptr->set_PODptr_using_data(pod_deall_ptr, result);
		pod_deall_ptr->set_data(result);

		return  result;
	}
	else{
		//	change values of allocated PODptr
		char status = pod_ptr->status();
		pod_ptr->set_total_size(0);
		pod_ptr->set_status(PODcontroller::ALLOC);
		m_alloc.push(pod_ptr);

		// 	get and change values of deallocated PODptr
		my_PODptr* pod_deall_ptr = m_dealloc.pop();
		pod_deall_ptr->set_total_size(total_size);

		// check last PODptr in allocated data
		if (status & PODcontroller::LAST)
			pod_deall_ptr->set_status(PODcontroller::BUSY | PODcontroller::LAST);
		else
			pod_deall_ptr->set_status(PODcontroller::BUSY);
		pod_deall_ptr->set_PODptr_using_data(pod_deall_ptr, result);
		pod_deall_ptr->set_data(result);

		return  static_cast<void*>(result);
	}
}

// deallocatint of allocated memory
void MemoryManager::deallocate (void* data_void_ptr){
	// check nullptr
	if (data_void_ptr == nullptr)
		return;

	// get pointer to PODptr using begining of element area 
	char* data_ptr = static_cast<char*> (data_void_ptr);
	char* data;
	my_PODptr* pod_ptr;
	unsigned bad_invalidate_count = 0;
	while (true){
		pod_ptr = my_PODptr::PODptr_using_data(data_ptr)->load(std::memory_order_relaxed);
		if (pod_ptr != nullptr){
			// invalidate m_ptr for data_ptr
			data = pod_ptr->invalidate();
			if (data != nullptr)
				break;
		}
		sleep(sleep_invalidate);
		++bad_invalidate_count;
		if (bad_invalidate_count > bad_invalidate_limit)
			throw std::runtime_error("my_PODptr::try_invalidate lock error");
	}

	// check data_ptr from 	my_PODptr* and data_ptr (must be the same) 
	if (data != data_ptr){
		if (my_PODptr::PODptr_using_data(data_ptr)->load(std::memory_order_relaxed) == pod_ptr){
			pod_ptr->set_data(data);
			throw std::runtime_error("wrond value for deallocate");
		}
		else{
			pod_ptr->set_data(data);
			deallocate(data_ptr);
		}
	}

	// check status (must be BUSY and not ALLOC)
	char status = pod_ptr->status();
	if (status & PODcontroller::ALLOC 
		||!(status & PODcontroller::BUSY)){
		pod_ptr->set_data(data);
		throw std::runtime_error("MemoryManager::deallocate wrong status");
	}
	size_t total_size = pod_ptr->total_size();
	pod_ptr->set_total_size(0);

	// push back my_PODptr* to m_dealloc stack
	pod_ptr->set_PODptr_using_data(nullptr, data);
	
	pod_ptr->set_status(PODcontroller::NO_STATUS);
	atomic_thread_fence(std::memory_order_release);
	m_dealloc.push(pod_ptr);

	// pop 	my_PODptr* from m_alloc stack
	my_PODptr* new_pod_ptr = m_alloc.pop();
	new_pod_ptr->set_total_size(total_size);

	// check my_PODptr* for LAST
	if (status & PODcontroller::LAST){
		new_pod_ptr->set_status(PODcontroller::ALLOC | PODcontroller::LAST | PODcontroller::BUSY);
	}
	else{
		new_pod_ptr->set_status(PODcontroller::ALLOC | PODcontroller::BUSY);
		m_alloc.marge_PODptr(new_pod_ptr, data);
	} 
	new_pod_ptr->set_PODptr_using_data(new_pod_ptr, data);
	new_pod_ptr->set_data(data);
}
#endif
