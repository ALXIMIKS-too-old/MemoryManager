
#ifndef SPINLOCKED_STACK
#define SPINLOCKED_STACK

#include <stack>
#include "spinlock.h"
#include <list>
#include <vector>

// thread safe stack besed on spinLock
template<class T>
class spinLockedStack
{
protected:
	typedef std::stack<T, std::vector<T>> stack_type;
	spinLock m_lock;
	stack_type m_stack;
	std::atomic<bool> m_empty;
public:

	spinLockedStack() : m_empty(true){}

	void push(T data){
		m_lock.lock();
		m_stack.push(data);
		m_lock.unlock();
		m_empty.store(false, std::memory_order_release);
	}

	T pop(){
		m_lock.lock();
		if (m_stack.empty()){
			m_lock.unlock();
			m_empty.store(true, std::memory_order_release);
			return nullptr;
		}
		T top = m_stack.top();
		m_stack.pop();
		m_lock.unlock();
		return top;
	}

	bool update_condition(const std::memory_order& memory_order = std::memory_order_relaxed){
		return m_empty.load(memory_order);
	}
};

// slower spinLockedStack analog on 30%
// based on list  (c мелкогранулярными блокировками)
template<class T>
class spinLockedStack_1
{
private:
	spinLock m_lock;
	std::atomic<bool> m_empty = true;

	struct Node{
		std::shared_ptr<T> m_data;
		std::unique_ptr<Node> m_next = nullptr;
		Node(T data) : m_data(std::make_shared<T>(data)) {}
		Node() = default;
	};
	std::unique_ptr<Node> head = nullptr;

public:
	void push(T data){
		std::unique_ptr<Node> new_node(new Node(std::move(data)));

		m_lock.lock();
		if (head != nullptr)
			new_node->m_next = std::move(head);
		head = std::move(new_node);
		m_lock.unlock();
		m_empty.store(false, std::memory_order_release);
	}

	std::shared_ptr<T> pop(){
		m_lock.lock();
		if (head == nullptr){
			m_lock.unlock();
			m_empty.store(true, std::memory_order_release);
			return std::shared_ptr<T>();
		}
		std::shared_ptr<T> data = head->m_data;
		head = std::move(head->m_next);
		m_lock.unlock();
		return data;
	}

	bool update_condition(const std::memory_order& memory_order = std::memory_order_acquire){
		return m_empty.load(memory_order);
	}
};

#endif