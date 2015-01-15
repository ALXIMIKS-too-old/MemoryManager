
#ifndef LISTED_VECTOR
#define LISTED_VECTOR

#include "PODptr.h"
#include <vector>
#include <memory>
#include <algorithm>

template<typename T, size_t vector_size = 32>
class listed_vector
{	
public:
typedef std::vector<T> vect;
typedef std::shared_ptr<std::vector<T>>	 shared_vect;
	struct Node{
		shared_vect m_data;
		Node* m_next;
		std::atomic<char> bad_search_count;

		Node(size_t size, Node* n = 0)
			: m_data(std::make_shared<vect>(size)), m_next(n){}
	};

	static double vector_coefficient;

private:
	std::atomic<Node*> m_head;
	std::atomic<int> m_vectors_count;
public:
	listed_vector() : m_head(nullptr), m_vectors_count(0){}

	~listed_vector(){
		Node* m_next = m_head.load(std::memory_order_acquire);
		Node* current;
		while (m_next != nullptr){
			current = m_next;
			m_next = current->m_next;
			delete current;
		}
	}

	Node* add_node(){  
		size_t next_size;
		Node *head = get_head(std::memory_order_acquire);
		if (head == nullptr)
			next_size = vector_size;
		else
			next_size = size_t(head->m_data->size() * vector_coefficient);

		std::unique_ptr<Node> new_node(new Node(next_size, head));
		while (!m_head.compare_exchange_strong(new_node.get()->m_next, new_node.get(),
			std::memory_order_release, std::memory_order_acquire));

		Node* node = new_node.release();
		++m_vectors_count;
		return node;
	}	 

	const int vector_count()const{
		return m_vectors_count;
	}

	Node* at(unsigned num)const{
		unsigned count = 0;
		Node* next_node;
		for (next_node = get_head(std::memory_order_acquire); next_node != nullptr && count < num; ++count)
			next_node = next_node->m_next;
		return next_node;
	}	 

	Node* get_head(const std::memory_order& memory_order = std::memory_order_relaxed)const{
		return m_head.load(memory_order);
	}
};

template<typename T, size_t vector_size>
double listed_vector<T, vector_size>::vector_coefficient = 2;

#endif