#pragma once
#pragma once
#include <cstdint>

#define MAX_NAME_SIZE 20

struct Data {
	char name_[MAX_NAME_SIZE]; //유일키
};

struct Node {
	Data data_;
	Node* prev_;
	Node* next_;
};

class CircularLinkedList {
private:
	Node* dummy_; // 더미 사용
public:
	CircularLinkedList();
	~CircularLinkedList();
	void add_node_back(Data* new_data);
	void add_node_front(Data* new_data);
	bool add_node_after(Data* new_data, Data* target_data);
	bool add_node_before(Data* new_data, Data* target_data);
	void clear_list(); 
	bool del_node(Data* target_data);
	bool del_node(Node* target_node);
	Node* search_node(Data* target_data);
	Node* get_dummy();
};