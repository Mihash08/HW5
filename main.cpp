#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <queue>
#include <chrono>
#include <iostream>
#include <utility>

pthread_mutex_t mutex_changing_queues ; // Мьютекс для изменений очередей
pthread_mutex_t mutex_sleep_section_1 ; // Мьютекс для засыпания первого продавца
pthread_mutex_t mutex_sleep_section_2 ; // Мьютекс для засыпания второго продовцы
pthread_mutex_t mutex_printing ;        // Мьютекс для вывода в консоль

pthread_cond_t cond_section_1;          // Условие для того, чтобы разбудить первого продавца
pthread_cond_t cond_section_2;          // Условие для того, чтобы разбудить второго продавца

struct ShoppingItem {
    bool first_section;
    int count;
    ShoppingItem() {
        first_section = rand() % 2;
        count = rand() % 4 + 4;
    }
};

struct Customer {
    // Вообще по-хорошему список покупок состоит из n пунктов, каждый из которых принадлежит первой или второй секции
    // Я же объединил m подряд идущих пункты принадлежащие одной секции в один пункт длины m
    // Это позволяет упростить алгоритм
    // После вычеркивания каждого пункта покупатель всегда переходит в другую очередь
    // или уходит из магазина, если список стал пуст.
    std::queue<ShoppingItem> shopping_list;
    std::string name;
    explicit Customer(std::string name) {
        this->name = std::move(name);
        for (int i = 0; i < rand() % 3 + 3; ++i) {
            shopping_list.push(ShoppingItem());
        }
    }
    Customer() = default;

    bool topSectionIsFirst() {
        return shopping_list.front().first_section;
    }
};

// Очереди покупателей в первой и второй секции
std::queue<Customer> queue_section_1;
std::queue<Customer> queue_section_2;

[[noreturn]] void *Seller(void *param) {
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

    // В самом начале работы программы все продавцы спят
    pthread_cond_wait(this_cond, this_mutex);

    while (true) {
        if (this_queue->empty()) {
            // Если очередь пустая, продавец выводит сообщение о том, что засыпает и засыпает, пока не получит сигнал
            // Как только он просыпается, он выводит об этом сообщение и продолжает работу
            pthread_mutex_lock(&mutex_printing);
            std::cout << "Seller in section " << this_section_name << " is out of customers and has gone to sleep\n";
            pthread_mutex_unlock(&mutex_printing);

            pthread_cond_wait(this_cond, this_mutex);

            pthread_mutex_lock(&mutex_printing);
            std::cout << "Seller in section " << this_section_name << " just got a customer and has woken up\n";
            pthread_mutex_unlock(&mutex_printing);
        } else {
            // Каждый покупатель обслуживатеся одну секнуду + по полсекунды за каждый пункт списка
            int wait_time = this_queue->front().shopping_list.front().count * 500 + 1000;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));

            pthread_mutex_lock(&mutex_changing_queues);
            // "Вычеркиваем" первый элемент из списка покупок
            this_queue->front().shopping_list.pop();
            pthread_mutex_lock(&mutex_printing);
            if (this_queue->front().shopping_list.empty()) {
                // Выводится, если список покупок кончился
                std::cout << this_queue->front().name << " has been served and has left the shop\n";
            } else {
                // Выводится, если в списке покупок еще есть элементы
                std::cout << this_queue->front().name << " has moved to section " << other_section_name << "\n";
                // В таком случае покупатель добавляется в другую очередь
                other_queue->push(this_queue->front());
                // Сигнал посылается на случай, если другой продавец уснул
                pthread_cond_signal(other_cond);
            }
            pthread_mutex_unlock(&mutex_printing);

            // Покупатель удаляется из сейчасшней очереди
            this_queue->pop();
            pthread_mutex_unlock(&mutex_changing_queues);
        }
    }
}

// Этот поток отвечает за добавление новый покупателей
[[noreturn]] void *CustomerManager(void *param) {
    Customer new_customer;
    // Очередь, в которую добавляется покупатель
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

        // Сигнал, чтобы разбудить продавца, если он спит
        pthread_cond_signal(this_condition);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000 + rand() % 10000));
    }
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
    pthread_create(&threadP[2],nullptr,CustomerManager, (nullptr));
    pthread_join(threadP[0], nullptr);
    pthread_join(threadP[1], nullptr);
    pthread_join(threadP[2], nullptr);

    pthread_mutex_destroy(&mutex_sleep_section_1);
    pthread_mutex_destroy(&mutex_sleep_section_2);
    pthread_mutex_destroy(&mutex_changing_queues);
    pthread_mutex_destroy(&mutex_printing);
    pthread_cond_destroy(&cond_section_1);
    pthread_cond_destroy(&cond_section_2);
    return 0;
}
