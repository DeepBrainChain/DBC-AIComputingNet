/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : list.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年1月20日
  最近修改   :
  功能描述   : common data structure: list
  函数列表   :
  修改历史   :
  1.日    期   : 2017年1月20日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _LIST_H_
#define _LIST_H_

#include "common.h"
#include "common_def.h"


//BE CAREFUL: 
//1. class T should override operator=, operator==
//2. list caller should clear data memory by itself


template<class T>
class __PRIVATE list_node
{
public:

    list_node(T &data)// : m_data(data), m_pre(NULL), m_next(NULL) 
    {
        m_data = data;
        m_pre = NULL;
        m_next = NULL;
    }

    virtual ~list_node() {m_pre = NULL; m_next = NULL;}

    T & get_data() {return m_data;}

    void set_data(volatile T &data) {m_data = data;}

    list_node *get_pre() {return m_pre;}

    list_node *get_next() {return m_next;}

    void set_pre(list_node *pre) {m_pre = pre;}

    void set_next(list_node *next) {m_next = next;}

protected:

    T m_data;
    
    list_node<T> *m_pre, *m_next;

private:

    list_node(const list_node<T> &);
    list_node &operator=(const list_node<T> &);
};

//!!list caller should clear data memory by itself
//BE CAREFUL: class T must override operator == 

template<class T>
class __EXPORT list
{
protected:

    enum
    {
        ONLY_ONE = 1
    };

public:
    list() : m_size(0), m_head(NULL), m_tail(NULL) {}

    virtual ~list() {remove_all();}

    uint32_t size() const {return m_size;}

    bool empty() const {return 0 == m_size;}

    T *get_head() {return (0 == m_size) ? NULL : &(m_head->get_data());}

    T *get_tail() {return (0 == m_size) ? NULL : &(m_tail->get_data());}
    
    int32_t add_tail(T &data)
    {
        //alloc new node
        list_node<T> *new_node = new list_node<T>(data);
        new_node->set_data(data);

        if (empty())
        {
            m_head = new_node;
            m_tail = new_node;
        }
        else
        {
            new_node->set_pre(m_tail);
            m_tail->set_next(new_node);

            m_tail = new_node;
        }

        m_size++;

        return 0;
    }

    int32_t add_head(T &data)
    {
        list_node<T> *new_node = new list_node<T>(data);
        new_node->set_data(data);

        if (empty())
        {
            m_head = new_node;
            m_tail = new_node;
        }
        else
        {
            new_node->set_next(m_head);
            m_head->set_pre(new_node);

            m_head = new_node;
        }

        m_size++;

        return 0;
    }

    void remove_tail()
    {
        if (empty())
        {
            return;
        }

        list_node<T> * p = m_tail;

        if (ONLY_ONE == m_size)
        {
            init_data();
        }
        else
        {
            m_tail = m_tail->get_pre();
            m_tail->set_next(NULL);

            m_size--;
        }

        //clear
        DELETE(p);     
    }

    void remove_head()
    {
        if (empty())
        {
            return;
        }

        list_node<T> * p = m_head;

        if (ONLY_ONE == m_size)
        {            
            init_data();
        }
        else
        {
            m_head = m_head->get_next();
            m_head->set_pre(NULL);

            m_size--;
        }

        //clear
        DELETE(p);
    }
    
    POSITION get_head_position( ) const
    {
        return (POSITION)m_head;
    }

    POSITION get_tail_position( ) const
    {
        return (POSITION)m_tail;
    }
    
    T & get_next(POSITION & position)
    {
        list_node<T> *node = (list_node<T> *)(position);
        
        T & data = node->get_data();
        position = (POSITION)node->get_next();
        
        return data;
    }
    
    T get_next(POSITION& position) const
    {
        list_node<T> *node = (list_node<T> *)(position);
        
        T & data = node->get_data();
        position = (POSITION)node->get_next();
        
        return data;
    }

    T& get_pre(POSITION& position)
    {
        list_node<T> *node = (list_node<T> *)(position);
        
        T & data = node->get_data();
        position = (POSITION)node->get_pre();
        
        return data;
    }
    
    T get_pre(POSITION& position) const
    {
        list_node<T> *node = (list_node<T> *)(position);
        
        T & data = node->get_data();
        position = (POSITION)node->get_pre();
        
        return data;
    }
    
    T & get_at(POSITION position)
    {
        list_node<T> *node = (list_node<T> *)(position);
        return node->get_data();
    }
    
    T get_at(POSITION position) const
    {
        list_node<T> *node = (list_node<T> *)(position);
        return node->get_data();
    }
    
    void set_at(POSITION position, T &new_data)
    {
        list_node<T> *node = (list_node<T> *)(position);
        node->set_data(new_data);
    }

    void remove_at(POSITION position)
    {
        list_node<T> *node = (list_node<T> *)(position);

        list_node<T> *pre = node->get_pre();
        list_node<T> *next = node->get_next();

        pre->set_next(next);
        next->set_pre(pre);

        DELETE(node);
    }

//    POSITION InsertBefore( POSITION position, T newElement )
//    {
//        
//    }
//
//    POSITION InsertAfter( POSITION position, T newElement )
//    {
//        
//    }
    
    POSITION find( T search_value, POSITION start_after = NULL) const
    {
        if (empty())
        {
            return (POSITION)NULL;
        }

        if (NULL == start_after)
        {
            start_after = m_head;
        }

        list_node<T> *node = (list_node<T> *)(start_after);

        while(node)
        {
            T &data = node->get_data();

            //BE CAREFUL: override operator == 
            if (data == search_value)
            {
                return (POSITION)node;
            }
            else
            {
                node = node->get_next();
            }
        }

        return (POSITION)NULL;
    }


    //nIndex begins from 0.
    POSITION find_index( uint32_t index ) const
    {
        if (empty())
        {
            return (POSITION)NULL;
        }
    
        uint32_t i = 0;
        list_node<T> *node = m_head;

        while(i < index && node)
        {
            i++;
            node = node->get_next();
        }

        return (NULL == node) ? (POSITION)NULL : (POSITION)node;
    }

    void remove_all()
    {        
        list_node<T> *node = m_head;

        while(node)
        {
            list_node<T> * delete_node = node;
            node = node->get_next();

            DELETE(delete_node);
        }
    }

protected:

    void init_data()
    {
        m_head = NULL;
        m_tail = NULL;
        
        m_size = 0;
    }

    uint32_t m_size;
    list_node<T> *m_head, *m_tail;

private:

    list(const list<T> &);
    list &operator=(const list<T> &);

};


#endif




