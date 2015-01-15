
#include <iostream>
#include <future>
#include <assert.h>
#include "My_Allocator.h"

static const size_t MB = 1024 * 1024;

static const int THREADss = 20;
static const unsigned count_alloc_dealloc = 10000;
static const unsigned min_alloc_size = 1;
static const unsigned max_alloc_size = 35;
static const unsigned pool_size = 200;

void test_MemoryManager_alloc_dealloc(MemoryManager& mem_manager, int THREADs, bool mode_pool = true);
void test_MemoryManager_allocator(int  count_alloc_dealloc);
void test_MemoryManager_one_thread(int count, size_t mem_size);

int main()
{
	test_MemoryManager_one_thread(10, 1000);
	test_MemoryManager_allocator(5000);

	// multy  thread  test of  MemoryManager
	std::cout << "\n\nmin_alloc_size:  " << min_alloc_size << std::endl;
	std::cout << "max_alloc_size:  " << max_alloc_size << std::endl;
	std::cout << "pool_size:  " << pool_size << std::endl;
	std::cout << "test_MemoryManager_alloc_dealloc:" << std::endl << std::endl;

	for (int i = THREADss; i > 0; --i){
		MemoryManager mem_manager(pool_size * MB);
		test_MemoryManager_alloc_dealloc(mem_manager, i);			  // with pool
		//test_MemoryManager_alloc_dealloc(mem_manager, i, false);		  // with new delete
	}
	int pause;
	std::cout << "The End" << std::endl;
	std::cin >> pause;
	return 0;
}

void test_MemoryManager_alloc_dealloc(MemoryManager& mem_manager, int THREADs, bool mode_pool){

	// create promise array and future array
	std::unique_ptr<std::promise<void>[]> promise_muss(new std::promise<void>[THREADs]);
	std::unique_ptr<std::future<void>[]> future_muss(new std::future<void>[THREADs]);

	// promise and shared_future for preparing of other threads
	std::promise<void> go;
	std::shared_future<void> ready(go.get_future());

	try{
		for (int i = THREADs - 1; i >= 0; --i){
			future_muss[i] = std::async(std::launch::async,
				[&mem_manager, ready, &promise_muss, i, mode_pool](){

				// send signal and wait answer;
				promise_muss[i].set_value();
				ready.wait();

				// work
				for (int j = count_alloc_dealloc - 1; j >= 0; --j){
					if (mode_pool){
						void* ptr = mem_manager.allocate(
							(min_alloc_size + (std::rand() % (max_alloc_size - min_alloc_size + 1))) * MB);
						if (ptr == nullptr)
							std::cout << "mem_manager nullptr" << std::endl;
						mem_manager.deallocate(ptr);
					}
					else{
						auto ptr = new char[((min_alloc_size + (std::rand() % (max_alloc_size - min_alloc_size + 1))) * MB)];
						if (ptr == nullptr)
							std::cout << "new dell nullptr" << std::endl;
						delete ptr;
					}
				}
			});
		}

		// wait all signals from threads
		for (int i = THREADs - 1; i >= 0; --i){
			promise_muss[i].get_future().wait();
		}
		// start
		unsigned int start_time = clock();
		go.set_value();

		// end
		for (int i = THREADs - 1; i >= 0; --i){
			future_muss[i].get();
		}
		unsigned int end_time = clock();
		if (mode_pool)
			std::cout << "MemoryManager thread - " << THREADs << " time: " << end_time - start_time << std::endl;
		else
			std::cout << "NEW DELETE thread - " << THREADs << " time: " << end_time - start_time << std::endl;

	}
	catch (std::exception& except){
		//go.set_value();
		std::cout << "\nerror - " << except.what() << std::endl;
	}
	catch (...){
		//go.set_value();
		std::cout << "\nerror - " << std::endl;
	}
}

void test_MemoryManager_allocator(int  count_alloc_dealloc){
	// create a different pools
	std::cout << "\ntest_MemoryManager_allocator:" << std::endl;

	std::vector<int, My_allocator<int, 0>> vector;
	std::vector<int, My_allocator<int, 1>> vector_1;
	std::vector<int, My_allocator<int, 2>> vector_2;

	try{
		// Test vector
		for (int i = count_alloc_dealloc; i >= 0; --i){
			vector.push_back(i);
			int xx = vector.back();
			assert(xx == i);
		}
		std::cout << "1.. ";

		for (int i = 0; i <= count_alloc_dealloc; ++i){
			assert(vector.back() == i);
			vector.pop_back();
		}
		std::cout << "2.. ";

		// Test vector_1
		for (int i = count_alloc_dealloc; i >= 0; --i){
			vector_1.push_back(i);
			assert(vector_1.back() == i);
		}
		std::cout << "3.. ";

		// Test vector_2
		for (int i = 0; i <= count_alloc_dealloc; ++i){
			vector_2.push_back(i);
			assert(vector_2.back() == i);
		}
		std::cout << "4.. ";

		// Test vector_2   with vector_1
		for (int i = 0; i <= count_alloc_dealloc; ++i){
			assert(vector_1[i] == vector_2[count_alloc_dealloc - i]);
		}
		std::cout << "\ntest_MemoryManager_allocator: done" << std::endl;
	}
	catch (std::exception& except){
		std::cout << "\ntest_MemoryManager_allocator: error - " << except.what() << std::endl;
	}
	catch (...){
		std::cout << "\ntest_MemoryManager_allocator: error" << std::endl;
	}
}

void test_MemoryManager_one_thread(int count, size_t mem_size)
{
	std::srand(static_cast<unsigned>(time(nullptr)));

	MemoryManager a(mem_size);

	void** muss = new void*[count];
	try{
		std::cout << "\ntest_MemoryManager_one_thread:" << std::endl;
		for (int x = count - 1; x >= 0; --x)	{
			muss[x] = a.allocate(1);
			if (muss[x] == nullptr)
				std::cout << "nullptr" << std::endl;
		}
		std::cout << "1.. ";

		for (int x = count - 1; x >= 0; --x)	{
			a.deallocate(muss[x]);
		}
		std::cout << "2.. ";

		for (int x = count - 1; x >= 0; --x)	{
			muss[x] = a.allocate(1);
			if (muss[x] == nullptr)
				std::cout << "nullptr" << std::endl;
		}
		std::cout << "3.. ";

		for (int x = count - 1; x >= 0; --x)	{
			a.deallocate(muss[x]);
		}
		std::cout << "4.. ";

		for (int x = count - 1; x >= 0; --x)	{
			muss[x] = a.allocate(1);
			if (muss[x] == nullptr)
				std::cout << "nullptr" << std::endl;
		}
		std::cout << "5.. ";

		for (int x = count - 1; x >= 0; --x)	{
			a.deallocate(muss[x]);
		}

		delete[] muss;
		std::cout << "\ntest_MemoryManager_one_thread: done" << std::endl;
	}
	catch (std::exception& except){
		delete[] muss;
		std::cout << "\ntest_MemoryManager_one_thread: error - " << except.what() << std::endl;
	}
	catch (...){
		delete[] muss;
		std::cout << "\ntest_MemoryManager_one_thread: error" << std::endl;
	}

}