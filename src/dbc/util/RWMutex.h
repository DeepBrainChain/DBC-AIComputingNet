#ifndef DBC_RWMUTEX_H
#define DBC_RWMUTEX_H

#include <pthread.h>

class RwMutex {
    public:
        class ReadLock {
        public:
            explicit ReadLock(RwMutex& mutex):m_isLock(false), m_mutex(mutex) {
                lock();
            }

            ~ReadLock() {
                if(m_isLock) {
                    unlock();
                }
            }

            void lock() {
                if(!m_isLock) {
                    m_isLock = true;
                    m_mutex.rdlock();
                }
            }

            void unlock() {
                if(m_isLock) {
                    m_mutex.unlock();
                    m_isLock = false;
                }
            }
        private:
            bool m_isLock;
            RwMutex& m_mutex;
        };

        class WriteLock {
        public:
            explicit WriteLock(RwMutex& mutex):m_isLock(false), m_mutex(mutex) {
                lock();
            }

            ~WriteLock() {
                if(m_isLock) {
                    unlock();
                }
            }

            void lock() {
                if(!m_isLock) {
                    m_isLock = true;
                    m_mutex.wrlock();
                }
            }

            void unlock() {
                if(m_isLock) {
                    m_mutex.unlock();
                    m_isLock = false;
                }
            }
        private:
            bool m_isLock;
            RwMutex& m_mutex;
        };

        RwMutex() {
            pthread_rwlock_init(&m_lock, nullptr);
        }

        ~RwMutex() {
            pthread_rwlock_destroy(&m_lock);
        }

        void rdlock() {
            pthread_rwlock_rdlock(&m_lock);
        }

        void wrlock() {
            pthread_rwlock_wrlock(&m_lock);
        }

        void unlock() {
            pthread_rwlock_unlock(&m_lock);
        }
    private:
        pthread_rwlock_t m_lock;
    };

#endif
