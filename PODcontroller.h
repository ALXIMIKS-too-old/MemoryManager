
#ifndef POD_CONTROLLER
#define POD_CONTROLLER

#include "spinlocked_stack.h"
#include "PODptr.h"
#include "listed_vector.h"
#include <algorithm>			  

static const unsigned sleep_update = 50; 
static const unsigned sleep_bad_search = 10;
static char bad_search_limit = 9;

static const size_t listed_vector_size = 32;
static char bad_search_for_marge = 2;
static size_t marge_minimal_size = 1024;


// class of main logic
// contain listed_vector container of PODptr objects
// also contain thread safe stack with pointer to non-using  PODptr objects	of 	listed_vector
// any PODptr objects has its own state (PODptr_status)
// realized search PODptr objects with needed element area size 
// realized marge of PODptr objects with empty neighboring element areas 
class PODcontroller{
	typedef my_PODptr PODptr;
	typedef listed_vector<PODptr, listed_vector_size> listed_PODptr;
	typedef spinLockedStack<PODptr*> stack_PODptr;

	stack_PODptr m_stack;
	listed_PODptr m_listed_vect;	
	char m_alloc_dealloc_status;

	std::mutex m_mutex;

	PODcontroller& operator= (const PODcontroller&) = delete;
	PODcontroller(const PODcontroller&) = delete;
	PODcontroller(PODcontroller&&) = delete;
	
public:
	// any PODptr objects has its own state 
	enum PODptr_status{
		// flags for PODptr::status
		NO_STATUS = 0,
		ALLOC = 1,	// role of PODptr:   ALLOC/DEALLOC
		BUSY = 2,   // current state of PODptr:  FREE/BUSY
		LAST = 4,   // last in pool
	};

	// PODcontroller with allocation functionality
	PODcontroller(char* begin_ptr, size_t size, PODptr_status alloc_dealloc_status = ALLOC) 
		: m_alloc_dealloc_status(alloc_dealloc_status){

		update();
		PODptr* pod_ptr = m_stack.pop();
																
		pod_ptr->set_total_size(size);
		pod_ptr->set_status(ALLOC | LAST | BUSY);
		pod_ptr->set_PODptr_using_begin_data(pod_ptr, begin_ptr);
		pod_ptr->set_begin(begin_ptr, std::memory_order_release);
	}

	// default PODcontroller constructor
	PODcontroller(PODptr_status alloc_dealloc_status = NO_STATUS){
		update();
	}

	// push to the thread safe stack non-using PODptr object
	void push(PODptr *pod){
		m_stack.push(pod);
	}

	// get pointer to non-using PODptr object from thread safe stack 
	PODptr* pop(){
		PODptr* result;
		while ((result = m_stack.pop()) == nullptr){
			update();
		}
		return result;
	}

	// find  PODptr object with needed size of element area and invalidate PODptr object
	std::pair<char*, PODptr*> find_size_in_listed_vector(size_t need_size){
		return _find_size_in_listed_vector(need_size);
	}

	// check size of element area and invalidate PODptr object
	char* try_lock_POD(size_t need_size, PODptr* pod_ptr){	
		char* result;
		if (pod_ptr->total_size() >= need_size)
			// try compare_exchange_week for data pointer with save of last value;
			if ((result = pod_ptr->try_invalidate()) != nullptr)
				// check size of current PODptr	2-nd time (must left more atomic pointer size)
				if (pod_ptr->total_size() >= need_size)
						return result;
				else pod_ptr->set_data(result, std::memory_order_release);
		return nullptr;
	}

	// check min size of element area and try to marge
	bool try_marge_POD(size_t need_size, PODptr* pod_ptr){
		char* data;
		if (pod_ptr->total_size() > need_size){
			// try compare_exchange_week for data pointer with save of last value;
			if ((data = pod_ptr->try_invalidate()) != nullptr){
				bool result = marge_PODptr(pod_ptr, data);
				pod_ptr->set_data(data, std::memory_order_release);
				return result;
			}
		}
		return false;
	}

	//marge of PODptr objects with empty neighboring element areas
	bool marge_PODptr(my_PODptr* pod_ptr_start, char* data){
		size_t marged_size = 0;

		// must be invalidated
		if (pod_ptr_start->valid(std::memory_order_acquire)){
			return false;
		}

		// check status
		char next_status = pod_ptr_start->status();
		if ((next_status & PODcontroller::LAST)
			|| !(next_status & PODcontroller::ALLOC)
			|| !(next_status & PODcontroller::BUSY))
			return false;

		char* next_data;

		my_PODptr* next = my_PODptr::PODptr_using_data(data + pod_ptr_start->total_size())->load();
		
		while (true){
			// check my_PODptr* ptr
			if (next == nullptr)
				break;

			// try invalidate for block
			next_data = next->try_invalidate();
			if (next_data == nullptr)
				break;

			// second check of status
			next_status = next->status();
			if (!(next_status & PODcontroller::ALLOC) || !(next_status & PODcontroller::BUSY)){
				next->set_data(next_data);
				break;
			}

			// push back my_PODptr* to m_alloc stack
			next->set_PODptr_using_data(nullptr, next_data);
			size_t total_size = next->total_size();
			marged_size += total_size;
			next->set_total_size(0);
			next->set_status(PODcontroller::ALLOC);
			push(next);

			// check last
			if (next_status & PODcontroller::LAST){
				pod_ptr_start->set_total_size(pod_ptr_start->total_size() + marged_size);
				pod_ptr_start->set_status(PODcontroller::ALLOC | PODcontroller::LAST | PODcontroller::BUSY);
				return marged_size > 0;
			}

			// get next  my_PODptr
			next = my_PODptr::PODptr_using_data(next_data + total_size)->load();
		}
		pod_ptr_start->set_total_size(pod_ptr_start->total_size() + marged_size);
		return marged_size > 0;
	}


protected:
	// update of listed_vector is thread safe steck is empty
	void update(){
		// check conditions for update (after lock)
		if (m_mutex.try_lock()){
			if (m_stack.update_condition()){
				std::vector<PODptr>& update_resource = *(m_listed_vect.add_node()->m_data);
				for (auto &i : update_resource){
					m_stack.push(&i);
					i.set_status(m_alloc_dealloc_status, std::memory_order_release);
				}
			}
			m_mutex.unlock();
		}
		else
			sleep(sleep_update);
	}

	// serching of needed size in listed_vector
	std::pair<char*, PODptr*> _find_size_in_listed_vector(size_t need_size){
		// randomizate of search
		std::srand(static_cast<unsigned>(time(nullptr)));
		int start_search_index = std::rand() % m_listed_vect.vector_count();

		listed_PODptr::Node* start_node = m_listed_vect.at(start_search_index);
		if (start_node == nullptr)
			throw std::runtime_error("wrong work of listed_vect::vector_count()");

		// random access for search in listed vector
		char bad_search_count = 0;
		do{
			auto res1 = find_size_in_Node_range(need_size, start_node, nullptr, bad_search_count);
			if (res1.first != nullptr)
				return std::move(res1);
			auto res2 = find_size_in_Node_range(need_size, m_listed_vect.get_head(), start_node, bad_search_count);
			if (res2.first != nullptr)
				return std::move(res2);
			sleep(sleep_bad_search);
		} while (++bad_search_count < bad_search_limit);
		return std::make_pair(nullptr, nullptr);
	}


	// serching of needed size in Node
	std::pair<char*, PODptr*> find_size_in_Node_range(
		size_t need_size,
		listed_PODptr::Node* begin,
		listed_PODptr::Node* end,
		char number_of_try)
	{
		char* result;
		for (listed_PODptr::Node* next_node = begin; next_node != end; next_node = next_node->m_next){
			int bad_search = next_node->bad_search_count.load(std::memory_order_acquire);
			if (number_of_try < bad_search)
				if (bad_search < bad_search_limit - 2)
					continue;

			// random access for search in vector
			auto next_pod_ptr = next_node->m_data->begin() + std::rand() % next_node->m_data->size();
			for (auto it_next = next_pod_ptr, it_end = next_node->m_data->end(); it_next != it_end; ++it_next){
				if ((result = try_lock_POD(need_size, &*it_next)) != nullptr){
					next_node->bad_search_count.store(0);
					return std::make_pair(result, &*it_next);
				}
			}
			for (auto it_next = next_node->m_data->begin(), it_end = next_pod_ptr; it_next != it_end; ++it_next){
				if ((result = try_lock_POD(need_size, &*it_next)) != nullptr){
					next_node->bad_search_count.store(0);
					return std::make_pair(result, &*it_next);
				}
			}
			
			++(next_node->bad_search_count);

			// try to marge all big element areas
			if (number_of_try % bad_search_for_marge == 0){
				{
					bool right_marge = false;
					for (auto it_next = next_node->m_data->begin(), it_end = next_node->m_data->end(); it_next != it_end; ++it_next)
					if (try_marge_POD(marge_minimal_size, &*it_next))
						right_marge = true;

					if (right_marge)
						next_node->bad_search_count.store(0);
				}
			}
		}
		return std::make_pair(nullptr, nullptr);
	}
};
#endif