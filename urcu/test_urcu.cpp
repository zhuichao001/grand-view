#include <iostream>
#include <thread>
#include <urcu-qsbr.h>


typedef struct { 
    int a;
    int b; 
}foo_t;

foo_t *gs_foo;

void thread_read() {
    foo_t *foo = nullptr;
    int sum = 0;

    rcu_register_thread();
    for (int i = 0; i < 65536; ++i) {
        for (int j = 0; j < 1024; ++j) {
            rcu_read_lock();
            foo = rcu_dereference(gs_foo);
            if (foo) {
                sum += foo->a + foo->b;
            }
            rcu_read_unlock();
        }
        rcu_quiescent_state();
    }
    rcu_unregister_thread();

    std::cout << "read sum:" << sum << std::endl;
}

void thread_write() {
    for (int i = 0; i < 65536; ++i) {
        for (int j = 0; j < 1024; ++j) {
            foo_t *foo = (foo_t*) malloc(sizeof(foo_t));
            foo->a = i+1;
            foo->b = i+2;
            rcu_xchg_pointer(&gs_foo, foo);
            synchronize_rcu();
            if (foo) {
                free(foo);
            }
        }
    }
    std::cout << "write done" << std::endl;
}

int main(){
    gs_foo = new(foo_t);
    gs_foo->a=0;
    gs_foo->b=0;

    std::thread thr(thread_read);
    std::thread thw(thread_write);
    thr.join();
    thw.join();

    std::cout<<"main done..."<<std::endl;
    return 0;
}
