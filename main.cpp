#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <random>
#include <thread>
#include <queue>
#include <chrono>
#include <iostream>
#include <condition_variable>

const int bufSize = 10;
int buf[bufSize] ; //буфер

int rear = 0 ; //индекс для записи в буфер
int front = 0 ; //индекс для чтения из буфера

sem_t  empty ; //семафор, отображающий насколько  буфер пуст
sem_t  full ; //семафор, отображающий насколько полон буфер

pthread_mutex_t mutex_changing_queues ;
pthread_mutex_t mutex_sleep_section_1 ;
pthread_mutex_t mutex_sleep_section_2 ;
pthread_mutex_t mutex_printing ;

pthread_cond_t cond_section_1;
pthread_cond_t cond_section_2;
struct ShoppingItem {
    bool first_section;
    int count;
    ShoppingItem() {
        first_section = rand() % 2;
        count = rand() % 4 + 4;
    }
};

struct Customer {
    std::queue<ShoppingItem> shopping_list;
    std::string name;
    Customer(std::string name) {
        this->name = name;
        for (int i = 0; i < rand() % 3 + 3; ++i) {
            shopping_list.push(ShoppingItem());
        }
    }
    Customer() {

    }
    bool topSectionIsFirst() {
        return shopping_list.front().first_section;
    }
};

std::queue<Customer> queue_section_1;
std::queue<Customer> queue_section_2;

void *Seller(void *param) {
    bool is_first_section = ((int*)param);
    pthread_mutex_t *this_mutex;
    pthread_cond_t *this_cond, *other_cond;
    std::queue<Customer> *this_queue;
    std::queue<Customer> *other_queue;
    std::string other_section_name;
    std::string this_section_name;
    if (is_first_section) {
        this_queue = &queue_section_1;
        other_queue = &queue_section_2;
        this_mutex = &mutex_sleep_section_1;
        this_cond = &cond_section_1;
        other_cond = &cond_section_2;
        other_section_name = "2";
        this_section_name = "1";
    } else {
        this_queue = &queue_section_2;
        other_queue = &queue_section_1;
        this_mutex = &mutex_sleep_section_2;
        this_cond = &cond_section_2;
        other_cond = &cond_section_1;
        other_section_name = "1";
        this_section_name = "2";
    }
    pthread_cond_wait(this_cond, this_mutex);
    while (true) {
        if (this_queue->empty()) {
            pthread_mutex_lock(&mutex_printing);
            std::cout << "Seller in section " << this_section_name << " is out of customers and has gone to sleep\n";
            pthread_mutex_unlock(&mutex_printing);
            pthread_cond_wait(this_cond, this_mutex);
            pthread_mutex_lock(&mutex_printing);
            std::cout << "Seller in section " << this_section_name << " just got a customer and has woken up\n";
            pthread_mutex_unlock(&mutex_printing);
        } else {
            int wait_time = this_queue->front().shopping_list.front().count * 500 + 1000;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            pthread_mutex_lock(&mutex_changing_queues);
            this_queue->front().shopping_list.pop();
            pthread_mutex_lock(&mutex_printing);
            if (this_queue->front().shopping_list.empty()) {
                std::cout << this_queue->front().name << " has been served and has left the shop\n";
            } else {
                std::cout << this_queue->front().name << " has moved to section " << other_section_name << "\n";
                other_queue->push(this_queue->front());
                pthread_cond_signal(other_cond);
            }
            pthread_mutex_unlock(&mutex_printing);
            this_queue->pop();
            pthread_mutex_unlock(&mutex_changing_queues);
        }
    }
    return nullptr;
}

void *CustomerManager(void *param) {
    Customer new_customer;
    std::queue<Customer> *this_queue;
    pthread_cond_t *this_condition;
    std::string this_section_name;
    int customer_count = 1;
    while (true) {
        new_customer = Customer("Customer_" + std::to_string(customer_count));
        if (new_customer.topSectionIsFirst()) {
            this_queue = &queue_section_1;
            this_condition = &cond_section_1;
            this_section_name = "1";
        } else {
            this_queue = &queue_section_2;
            this_condition = &cond_section_2;
            this_section_name = "2";
        }
        pthread_mutex_lock(&mutex_changing_queues);
        this_queue->push(new_customer);
        pthread_mutex_unlock(&mutex_changing_queues);

        pthread_mutex_lock(&mutex_printing);
        std::cout << "New customer " << new_customer.name << " joined queue in section " << this_section_name <<"\n";
        customer_count++;
        pthread_mutex_unlock(&mutex_printing);
        pthread_cond_signal(this_condition);
        std::this_thread::sleep_for(std::chrono::milliseconds(5000 + rand() % 4000));
    }
    return nullptr;
}

int main() {
    std::srand(std::time(nullptr));
    pthread_mutex_init(&mutex_changing_queues, nullptr);
    pthread_mutex_init(&mutex_sleep_section_1, nullptr);
    pthread_mutex_init(&mutex_sleep_section_2, nullptr);
    pthread_mutex_init(&mutex_printing, nullptr);
    pthread_cond_init(&cond_section_1, nullptr);
    pthread_cond_init(&cond_section_2, nullptr);
    pthread_t threadP[3] ;
    bool true_bool = true, false_bool = false;

    pthread_create(&threadP[0],nullptr,Seller, (void*)(true_bool));
    pthread_create(&threadP[1],nullptr,Seller, (void*)(false_bool));
    pthread_create(&threadP[2],nullptr,CustomerManager, (NULL));
    pthread_join(threadP[0], NULL);
    pthread_join(threadP[1], NULL);
    pthread_join(threadP[2], NULL);

    pthread_mutex_destroy(&mutex_sleep_section_1);
    pthread_mutex_destroy(&mutex_sleep_section_2);
    pthread_mutex_destroy(&mutex_changing_queues);
    pthread_mutex_destroy(&mutex_printing);
    pthread_cond_destroy(&cond_section_1);
    pthread_cond_destroy(&cond_section_2);
    return 0;
}