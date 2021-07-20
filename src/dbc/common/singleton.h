#ifndef DBCPROJ_SINGLETON_H
#define DBCPROJ_SINGLETON_H

template <typename T>
struct Singleton
{
public:
    static T& instance() {
        static T obj;
        create_object.do_nothing();
        return obj;
    }

private:
    struct object_creator {
        object_creator(){ Singleton<T>::instance(); }
        inline void do_nothing()const {}
    };

    static object_creator create_object;
};

template <typename T>
typename Singleton<T>::object_creator
        Singleton<T>::create_object;

#endif //DBCPROJ_SINGLETON_H
