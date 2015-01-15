
#include <mutex>
#include "MemoryManager.h"


// class of allocator for STL containers with _Version of pool area
// based on MemoryManager class
// for control of size of pool area using size_t My_allocator<T, _Version>::memory_size  static-variable
template <class T, unsigned _Version = 0> class My_allocator
{
public:
	typedef T                 value_type;
	typedef value_type*       pointer;
	typedef const value_type* const_pointer;
	typedef value_type&       reference;
	typedef const value_type& const_reference;
	typedef std::size_t       size_type;
	typedef std::ptrdiff_t    difference_type;

	static std::atomic<MemoryManager*> m_instance;
	static size_t memory_size;

	static std::mutex m_mutex;

	static MemoryManager* getInstance() {
		// In order is using "std::call_once with std::once_flag" 	   
		// http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
		// 
		static MemoryManager mem_man(memory_size);
		return &mem_man;
	}

	template <class U>
	struct rebind {
		typedef My_allocator<U, _Version> other;
	};

	My_allocator() {}
	My_allocator(const My_allocator&) {}

	template <class U>
	My_allocator(const My_allocator<U, _Version>&) {}
	~My_allocator() {}

	pointer address(reference x) const{
		return &x;
	}
	const_pointer address(const_reference x) const {
		return x;
	}

	pointer allocate(size_type n, const_pointer = 0) {

		void* p = getInstance()->allocate(n * sizeof(T));
		if (!p)
			throw std::bad_alloc();
		return static_cast<pointer>(p);
	}

	void deallocate(pointer p, size_type) {
		getInstance()->deallocate(p);
	}

	size_type max_size() const {
		return static_cast<size_type>(-1) / sizeof(T);
	}

	void construct(pointer p, const value_type& x) {
		new(p)value_type(x);
	}

	template <typename ..._Types>
	void construct(pointer p, _Types&&... _Args)
	{
		new(p)value_type(std::forward<_Types>(_Args)...);
	}

	void destroy(pointer p) {
		p->~value_type();
	}

private:
	void operator=(const My_allocator&);
};

template <class T, unsigned _Version>
size_t My_allocator<T, _Version>::memory_size = 1 * 1024 * 1024;

template <class T, unsigned _Version>
std::mutex My_allocator<T, _Version>::m_mutex;

template <class T, unsigned _Version>
std::atomic<MemoryManager*> My_allocator<T, _Version>::m_instance;


// void version
template<unsigned _Version> class My_allocator<void, _Version>
{
	typedef void        value_type;
	typedef void*       pointer;
	typedef const void* const_pointer;

	template <class U>
	struct rebind { typedef My_allocator<U, _Version> other; };
};


	template <class T, unsigned _Version1, unsigned _Version2>
	inline bool operator==(const My_allocator<T, _Version1>&,
		const My_allocator<T, _Version2>&) {
		return _Version1 == _Version2;
	}

	template <class T, unsigned _Version1, unsigned _Version2>
	inline bool operator !=(const My_allocator<T, _Version1>&,
		const My_allocator<T, _Version2>&) {
		return _Version1 != _Version2;
	}