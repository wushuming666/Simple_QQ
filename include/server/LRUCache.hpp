#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <unordered_map>
#include <muduo/net/TcpConnection.h>

using namespace std;
using namespace muduo;
using namespace muduo::net;

class Node {
public:
    int key;
    TcpConnectionPtr value;
    Node *prev, *next;

    Node(int k = 0, TcpConnectionPtr v = 0) : key(k), value(v) {}
};

class LRUCache {
public:
    LRUCache(int capacity) : capacity(capacity), dummy(new Node()){
        dummy->prev = dummy;
        dummy->next = dummy;
    }
    
    TcpConnectionPtr get(int key) {
        auto node = get_node(key);
        return node ? node->value : nullptr;
    }
    
    void put(int key, TcpConnectionPtr value) {
        auto node = get_node(key);
        if (node) {
            node->value = value;
            return;
        }
        key_to_node[key] = node = new Node(key, value);
        push_front(node);
        if (key_to_node.size() > capacity)
        {
            auto back_node = dummy->prev;
            key_to_node.erase(back_node->key);
            remove(back_node);
            delete back_node;
        }
    }

    // 将key值的删除
    void remove(int key)
    {
        Node* tmp = get_node(key);
        remove(tmp);
    }

private:
    int capacity;
    Node *dummy;
    unordered_map<int, Node*> key_to_node;

    // 删除一个节点 
    void remove(Node *x)
    {
        x->prev->next = x->next;
        x->next->prev = x->prev;
    }
    
    // 在链表头添加
    void push_front(Node *x)
    {
        x->prev = dummy;
        x->next = dummy->next;
        x->prev->next = x;
        x->next->prev = x;
    }

    Node *get_node(int key)
    {
        auto it = key_to_node.find(key);
        if (it == key_to_node.end()) return nullptr;
        auto node = it->second;
        remove(node);
        push_front(node);
        return node;
    }
};
#endif